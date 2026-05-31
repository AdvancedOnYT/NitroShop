#include "networking.hpp"
#include <3ds.h>
#include <malloc.h>
#include <curl/curl.h>

#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x400000 // 4MB SOC Buffer for faster throughput

static u32* soc_buffer = nullptr;
static bool soc_initialized = false;

bool Networking::init() {
    if (soc_initialized) return true;

    soc_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
    if (!soc_buffer) return false;

    Result rc = socInit(soc_buffer, SOC_BUFFERSIZE);
    if (R_FAILED(rc)) {
        free(soc_buffer);
        soc_buffer = nullptr;
        return false;
    }

    curl_global_init(CURL_GLOBAL_ALL);
    soc_initialized = true;
    return true;
}

void Networking::exit() {
    if (!soc_initialized) return;

    curl_global_cleanup();
    socExit();
    
    if (soc_buffer) {
        free(soc_buffer);
        soc_buffer = nullptr;
    }
    soc_initialized = false;
}

bool Networking::isConnected() {
    u32 wifiStatus = 0;
    if (R_SUCCEEDED(acInit())) {
        ACU_GetStatus(&wifiStatus);
        acExit();
    }
    return wifiStatus == 3; // 3 = connected
}

static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string* s = (std::string*)userp;
    try {
        s->append((char*)contents, totalSize);
    } catch (...) {
        return 0;
    }
    return totalSize;
}

bool Networking::downloadString(const std::string& url, std::string& outResult, const std::string& cookie) {
    if (!soc_initialized) return false;

    CURL* curl = curl_easy_init();
    if (!curl) return false;

    outResult.clear();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outResult);
    
    // SSL Verification settings (securely verifying using the bundled Root CAs)
    curl_easy_setopt(curl, CURLOPT_CAINFO, "romfs:/cacert.pem");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L); // 60s timeout for large catalog pages
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "NitroShop/1.0 (Nintendo 3DS)");
    curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1L);
    curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 65536L); // 64KB receive buffer

    if (!cookie.empty()) {
        curl_easy_setopt(curl, CURLOPT_COOKIE, cookie.c_str());
    }

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    return res == CURLE_OK;
}
