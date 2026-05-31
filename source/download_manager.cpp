#include "download_manager.hpp"
#include "file_system.hpp"
#include <curl/curl.h>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cstring>

struct ProgressData {
    DownloadManager* manager;
    uint64_t local_file_size;
    uint64_t header_content_length;
    double start_time;
    CURL* curl_handle;
};

static size_t headerCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
    ProgressData* data = static_cast<ProgressData*>(userdata);
    size_t total = size * nitems;
    size_t prefixLen = 15; // "Content-Length:"
    if (total >= prefixLen) {
        if (strncasecmp(buffer, "Content-Length:", prefixLen) == 0) {
            const char* valStart = buffer + prefixLen;
            while (valStart < buffer + total && (*valStart == ' ' || *valStart == '\t')) valStart++;
            if (valStart < buffer + total) {
                data->header_content_length = strtoull(valStart, nullptr, 10);
            }
        }
    }
    return total;
}

static int progressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    ProgressData* data = static_cast<ProgressData*>(clientp);
    
    if (data->manager->getStatus() == DownloadStatus::CANCELLED) {
        return 1;
    }
    
    curl_off_t effectiveTotal = dltotal;
    
    if (effectiveTotal <= 0 && data->header_content_length > 0) {
        effectiveTotal = static_cast<curl_off_t>(data->header_content_length - data->local_file_size);
        if (effectiveTotal < 0) effectiveTotal = 0;
    }
    
    if (effectiveTotal <= 0 && data->curl_handle) {
        curl_off_t cl = 0;
        if (curl_easy_getinfo(data->curl_handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &cl) == CURLE_OK && cl > 0) {
            effectiveTotal = cl - static_cast<curl_off_t>(data->local_file_size);
            if (effectiveTotal < 0) effectiveTotal = 0;
        }
    }
    
    if (effectiveTotal <= 0) {
        uint64_t knownSize = data->manager->getCurrentEntry().size;
        if (knownSize > data->local_file_size) {
            effectiveTotal = static_cast<curl_off_t>(knownSize - data->local_file_size);
        }
    }
    
    uint64_t total = data->local_file_size + effectiveTotal;
    uint64_t downloaded = data->local_file_size + dlnow;
    
    double now = osGetTime();
    double elapsed = (now - data->start_time) / 1000.0;
    
    double speed = 0;
    double eta = 0;
    if (elapsed > 0.5 && dlnow > 0) {
        speed = dlnow / elapsed;
        if (effectiveTotal > 0 && speed > 0) {
            uint64_t remaining = (effectiveTotal > dlnow) ? (effectiveTotal - dlnow) : 0;
            eta = static_cast<double>(remaining) / speed;
        }
    }
    
    data->manager->resetProgress(downloaded, (effectiveTotal > 0) ? total : 0, speed, eta);
    
    return 0;
}

static std::string sanitizeFilename(const std::string& title) {
    std::string name = title;
    const std::string invalidChars = "\\/:*?\"<>|";
    for (char& c : name) {
        if (invalidChars.find(c) != std::string::npos) {
            c = '_';
        }
    }
    return name + ".nds";
}

DownloadManager::DownloadManager()
    : status(DownloadStatus::IDLE), downloaded_bytes(0), total_bytes(0),
      download_speed(0), eta(0), cancel_requested(false), thread_running(false) {}

DownloadManager::~DownloadManager() {
    cancelDownload();
}

bool DownloadManager::startDownload(const CatalogEntry& entry, const std::string& target_dir, const std::string& cookie) {
    if (status == DownloadStatus::DOWNLOADING || thread_running) {
        return false;
    }
    
    status = DownloadStatus::DOWNLOADING;
    downloaded_bytes = 0;
    total_bytes = 0;
    download_speed = 0;
    eta = 0;
    cancel_requested = false;
    error_msg.clear();
    current_entry = entry;
    download_dir = target_dir;
    archive_cookie = cookie;
    
    thread_running = true;
    s32 priority = 0x38;
    thread = threadCreate(downloadThreadEntry, this, 256 * 1024, priority, -2, false);
    if (!thread) {
        status = DownloadStatus::ERROR;
        error_msg = "Failed to spawn download thread";
        thread_running = false;
        return false;
    }
    
    return true;
}

void DownloadManager::cancelDownload() {
    if (!thread_running) return;
    
    cancel_requested = true;
    status = DownloadStatus::CANCELLED;
    
    threadJoin(thread, U64_MAX);
    threadFree(thread);
    thread_running = false;
}

void DownloadManager::reset() {
    cancelDownload();
    status = DownloadStatus::IDLE;
    downloaded_bytes = 0;
    total_bytes = 0;
    download_speed = 0;
    eta = 0;
    cancel_requested = false;
    error_msg.clear();
}

void DownloadManager::downloadThreadEntry(void* arg) {
    static_cast<DownloadManager*>(arg)->runDownload();
}

void DownloadManager::resetProgress(uint64_t downloaded, uint64_t total, double speed, double eta_val) {
    downloaded_bytes = downloaded;
    total_bytes = total;
    download_speed = speed;
    eta = eta_val;
}

void DownloadManager::runDownload() {
    std::string safeName = sanitizeFilename(current_entry.title);
    std::string finalPath = download_dir;
    if (finalPath.back() != '/') finalPath += "/";
    finalPath += safeName;
    
    FileSystem::createDirectory(download_dir);
    
    std::string dlUrl = current_entry.url;
    if (dlUrl.compare(0, 8, "https://") == 0) {
        dlUrl = "http://" + dlUrl.substr(8);
    }
    
    char* file_buffer = (char*)malloc(128 * 1024);
    
    uint64_t localSize = 0;
    int max_retries = 3;
    int attempt = 0;
    CURLcode res = CURLE_OK;
    
    while (attempt <= max_retries && !cancel_requested) {
        FILE* file = nullptr;
        if (attempt == 0) {
            file = fopen(finalPath.c_str(), "wb");
            localSize = 0;
        } else {
            // Sleep 2 seconds before retrying
            svcSleepThread(2000000000ULL);
            localSize = FileSystem::getFileSize(finalPath);
            file = fopen(finalPath.c_str(), "ab");
        }
        
        if (!file) {
            status = DownloadStatus::ERROR;
            error_msg = "Failed to open target file for writing";
            if (file_buffer) free(file_buffer);
            thread_running = false;
            return;
        }
        
        if (file_buffer) {
            setvbuf(file, file_buffer, _IOFBF, 128 * 1024);
        }
        
        CURL* curl = curl_easy_init();
        if (!curl) {
            fclose(file);
            status = DownloadStatus::ERROR;
            error_msg = "Failed to initialize libcurl";
            if (file_buffer) free(file_buffer);
            thread_running = false;
            return;
        }
        
        ProgressData progData;
        progData.manager = this;
        progData.local_file_size = localSize;
        progData.header_content_length = 0;
        progData.start_time = osGetTime();
        progData.curl_handle = curl;
        
        curl_easy_setopt(curl, CURLOPT_URL, dlUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &progData);
        
        curl_easy_setopt(curl, CURLOPT_CAINFO, "romfs:/cacert.pem");
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
        curl_easy_setopt(curl, CURLOPT_SSL_SESSIONID_CACHE, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_ENABLE_ALPN, 0L);
        
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "NitroShop/1.0 (Nintendo 3DS)");
        
        curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1L);
        curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 131072L);
        curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L); // Unlimited timeout to support slow large downloads
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 100L);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 60L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 30L);
        
        if (localSize > 0) {
            curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, (curl_off_t)localSize);
        }
        
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Accept-Encoding: identity");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        
        if (!archive_cookie.empty()) {
            curl_easy_setopt(curl, CURLOPT_COOKIE, archive_cookie.c_str());
        }
        
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progData);
        
        res = curl_easy_perform(curl);
        
        fflush(file);
        fclose(file);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        
        if (res == CURLE_OK || cancel_requested) {
            break;
        }
        
        attempt++;
    }
    
    if (file_buffer) {
        free(file_buffer);
    }
    
    if (cancel_requested) {
        status = DownloadStatus::CANCELLED;
        FileSystem::deleteFile(finalPath); // Delete partial file
    } else if (res == CURLE_OK) {
        uint64_t actualSize = FileSystem::getFileSize(finalPath);
        if (current_entry.size > 0 && actualSize != current_entry.size) {
            status = DownloadStatus::ERROR;
            error_msg = "File size mismatch on completion";
            FileSystem::deleteFile(finalPath); // Delete corrupt file
        } else {
            status = DownloadStatus::COMPLETED;
        }
    } else {
        status = DownloadStatus::ERROR;
        error_msg = "Download failed: " + std::string(curl_easy_strerror(res));
        FileSystem::deleteFile(finalPath); // Delete incomplete file
    }
    
    thread_running = false;
}
