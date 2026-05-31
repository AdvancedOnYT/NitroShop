#include "catalog.hpp"
#include "networking.hpp"
#include "file_system.hpp"
#include "rom_sizes.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <cctype>

struct ArchiveSource {
    std::string url;
    std::string zip_name;
};

static const ArchiveSource SOURCES[] = {
    {"https://ia601903.us.archive.org/view_archive.php?archive=/1/items/ndsfull/Nintendo%20DS%200001-1000.zip", "Nintendo%20DS%200001-1000.zip"},
    {"https://ia801903.us.archive.org/view_archive.php?archive=/1/items/ndsfull/Nintendo%20DS%201001-2000.zip", "Nintendo%20DS%201001-2000.zip"},
    {"https://ia601903.us.archive.org/view_archive.php?archive=/1/items/ndsfull/Nintendo%20DS%202001-3000.zip", "Nintendo%20DS%202001-3000.zip"},
    {"https://ia601903.us.archive.org/view_archive.php?archive=/1/items/ndsfull/Nintendo%20DS%203001-4000.zip", "Nintendo%20DS%203001-4000.zip"},
    {"https://ia801903.us.archive.org/view_archive.php?archive=/1/items/ndsfull/Nintendo%20DS%204001-5000.zip", "Nintendo%20DS%204001-5000.zip"},
    {"https://ia601903.us.archive.org/view_archive.php?archive=/1/items/ndsfull/Nintendo%20DS%205001-6000.zip", "Nintendo%20DS%205001-6000.zip"},
    {"https://ia801903.us.archive.org/view_archive.php?archive=/1/items/ndsfull/Nintendo%20DS%206001-6619.zip", "Nintendo%20DS%206001-6619.zip"}
};

static std::string urlDecode(const std::string& str) {
    std::string result;
    result.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%' && i + 2 < str.size()) {
            std::string hex = str.substr(i + 1, 2);
            char chr = static_cast<char>(std::strtol(hex.c_str(), nullptr, 16));
            result += chr;
            i += 2;
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

static std::string extractJsonString(const std::string& json, const std::string& key) {
    size_t keyPos = json.find("\"" + key + "\"");
    if (keyPos == std::string::npos) return "";
    
    size_t colonPos = json.find(":", keyPos);
    if (colonPos == std::string::npos) return "";
    
    size_t quoteStart = json.find("\"", colonPos);
    if (quoteStart == std::string::npos) return "";
    
    size_t quoteEnd = json.find("\"", quoteStart + 1);
    if (quoteEnd == std::string::npos) return "";
    
    return json.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
}

static uint64_t extractJsonNumber(const std::string& json, const std::string& key) {
    size_t keyPos = json.find("\"" + key + "\"");
    if (keyPos == std::string::npos) return 0;
    
    size_t colonPos = json.find(":", keyPos);
    if (colonPos == std::string::npos) return 0;
    
    size_t digitStart = json.find_first_of("0123456789", colonPos);
    if (digitStart == std::string::npos) return 0;
    
    size_t digitEnd = json.find_first_not_of("0123456789", digitStart);
    std::string valStr;
    if (digitEnd == std::string::npos) {
        valStr = json.substr(digitStart);
    } else {
        valStr = json.substr(digitStart, digitEnd - digitStart);
    }
    
    try {
        return std::stoull(valStr);
    } catch (...) {
        return 0;
    }
}

static void parseHtmlArchive(const std::string& html, const std::string& zipName, std::vector<CatalogEntry>& outEntries) {
    size_t pos = 0;
    while (true) {
        size_t hrefPos = html.find("href=", pos);
        if (hrefPos == std::string::npos) break;
        
        pos = hrefPos + 5;
        if (pos >= html.size()) break;
        
        char quote = html[pos];
        if (quote != '"' && quote != '\'') continue;
        
        size_t start = pos + 1;
        size_t end = html.find(quote, start);
        if (end == std::string::npos) continue;
        
        std::string linkVal = html.substr(start, end - start);
        pos = end + 1;
        
        std::string lowerLink = linkVal;
        std::transform(lowerLink.begin(), lowerLink.end(), lowerLink.begin(), [](unsigned char c){ return std::tolower(c); });
        
        if (lowerLink.find(".nds") == std::string::npos) continue;
        
        std::string urlEncodedFilename;
        size_t fileParam = lowerLink.find("file=");
        if (fileParam != std::string::npos) {
            urlEncodedFilename = linkVal.substr(fileParam + 5);
            size_t amp = urlEncodedFilename.find('&');
            if (amp != std::string::npos) {
                urlEncodedFilename = urlEncodedFilename.substr(0, amp);
            }
        } else {
            size_t slash = linkVal.find_last_of('/');
            if (slash != std::string::npos) {
                urlEncodedFilename = linkVal.substr(slash + 1);
            } else {
                urlEncodedFilename = linkVal;
            }
        }
        
        std::string rawFilename = urlDecode(urlEncodedFilename);
        if (rawFilename.size() < 4 || rawFilename.substr(rawFilename.size() - 4) != ".nds") {
            continue;
        }
        
        // Prevent duplicate scanning
        bool exists = false;
        for (const auto& e : outEntries) {
            if (e.raw_filename == rawFilename) {
                exists = true;
                break;
            }
        }
        if (exists) continue;
        
        // Direct download URL: https://archive.org/download/ndsfull/ZIP_NAME/FILENAME
        std::string directUrl = "https://archive.org/download/ndsfull/" + zipName + "/" + urlEncodedFilename;
        
        CatalogEntry entry;
        entry.raw_filename = rawFilename;
        entry.url = directUrl;
        
        // Parse Title, ID, Region from Raw Filename
        std::string cleanName = rawFilename.substr(0, rawFilename.size() - 4); // strip .nds
        
        size_t firstDash = cleanName.find(" - ");
        if (firstDash != std::string::npos) {
            std::string idPart = cleanName.substr(0, firstDash);
            bool isNumeric = !idPart.empty() && std::all_of(idPart.begin(), idPart.end(), [](unsigned char c){ return std::isdigit(c); });
            if (isNumeric) {
                entry.id = idPart;
                cleanName = cleanName.substr(firstDash + 3);
            } else {
                entry.id = "N/A";
            }
        } else {
            entry.id = "N/A";
        }
        
        const auto& romSizes = getRomSizes();
        auto it = romSizes.find(cleanName);
        if (it != romSizes.end()) {
            entry.size = it->second;
        }
        
        size_t openParen = cleanName.find_last_of('(');
        size_t closeParen = cleanName.find_last_of(')');
        if (openParen != std::string::npos && closeParen != std::string::npos && closeParen > openParen && closeParen == cleanName.size() - 1) {
            entry.region = cleanName.substr(openParen + 1, closeParen - openParen - 1);
            std::string titlePart = cleanName.substr(0, openParen);
            while (!titlePart.empty() && (std::isspace(static_cast<unsigned char>(titlePart.back())) || titlePart.back() == '-')) {
                titlePart.pop_back();
            }
            entry.title = titlePart;
        } else {
            entry.region = "Unknown";
            entry.title = cleanName;
        }
        
        outEntries.push_back(entry);
    }
}

Catalog::Catalog() {
    for (int i = 0; i < 27; ++i) {
        jump_table[i] = -1;
    }
}

void Catalog::parseCachedCatalog(const std::string& jsonContent) {
    entries.clear();
    
    size_t pos = 0;
    while (true) {
        size_t start = jsonContent.find('{', pos);
        if (start == std::string::npos) break;
        
        size_t end = jsonContent.find('}', start);
        if (end == std::string::npos) break;
        
        std::string objStr = jsonContent.substr(start, end - start + 1);
        
        CatalogEntry entry;
        entry.raw_filename = extractJsonString(objStr, "raw_filename");
        entry.url = extractJsonString(objStr, "url");
        entry.title = extractJsonString(objStr, "title");
        entry.id = extractJsonString(objStr, "id");
        entry.region = extractJsonString(objStr, "region");
        entry.size = extractJsonNumber(objStr, "size");
        
        if (entry.size == 0 && !entry.raw_filename.empty()) {
            std::string lookupName = entry.raw_filename;
            if (lookupName.size() >= 4 && lookupName.substr(lookupName.size() - 4) == ".nds") {
                lookupName = lookupName.substr(0, lookupName.size() - 4);
            }
            size_t firstDash = lookupName.find(" - ");
            if (firstDash != std::string::npos) {
                std::string idPart = lookupName.substr(0, firstDash);
                bool isNumeric = !idPart.empty() && std::all_of(idPart.begin(), idPart.end(), [](unsigned char c){ return std::isdigit(c); });
                if (isNumeric) {
                    lookupName = lookupName.substr(firstDash + 3);
                }
            }
            const auto& romSizes = getRomSizes();
            auto it = romSizes.find(lookupName);
            if (it != romSizes.end()) {
                entry.size = it->second;
            }
        }
        
        if (!entry.title.empty() && !entry.url.empty()) {
            entries.push_back(entry);
        }
        
        pos = end + 1;
    }
}

static bool compareCaseInsensitive(const CatalogEntry& a, const CatalogEntry& b) {
    std::string strA = a.title;
    std::string strB = b.title;
    std::transform(strA.begin(), strA.end(), strA.begin(), [](unsigned char c){ return std::tolower(c); });
    std::transform(strB.begin(), strB.end(), strB.begin(), [](unsigned char c){ return std::tolower(c); });
    return strA < strB;
}

void Catalog::sortCatalog() {
    std::sort(entries.begin(), entries.end(), compareCaseInsensitive);
}

void Catalog::buildJumpTable() {
    for (int i = 0; i < 27; ++i) {
        jump_table[i] = -1;
    }
    
    for (size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].title.empty()) continue;
        
        char first = std::toupper(static_cast<unsigned char>(entries[i].title[0]));
        int slot = 0; // default for '#'
        if (first >= 'A' && first <= 'Z') {
            slot = first - 'A' + 1;
        }
        
        if (jump_table[slot] == -1) {
            jump_table[slot] = static_cast<int>(i);
        }
    }
    
    int lastValid = static_cast<int>(entries.size());
    for (int i = 26; i >= 0; --i) {
        if (jump_table[i] == -1) {
            jump_table[i] = lastValid;
        } else {
            lastValid = jump_table[i];
        }
    }
}

int Catalog::getJumpIndex(char letter) const {
    char first = std::toupper(static_cast<unsigned char>(letter));
    int slot = 0;
    if (first >= 'A' && first <= 'Z') {
        slot = first - 'A' + 1;
    }
    
    int idx = jump_table[slot];
    if (idx >= static_cast<int>(entries.size())) {
        return -1;
    }
    return idx;
}

bool Catalog::load(bool forceRefresh, AppConfig& config) {
    bool mustDownload = forceRefresh;
    
    bool cacheExists = FileSystem::fileExists("sdmc:/3ds/NitroShop/catalog.cache");
    if (!cacheExists) {
        mustDownload = true;
    }
    
    if (!mustDownload && cacheExists) {
        time_t now = time(nullptr);
        time_t diff = now - config.last_refresh;
        if (diff > 86400) { // 24 hours
            mustDownload = true;
        }
    }
    
    std::vector<CatalogEntry> newEntries;
    bool downloadSuccess = true;
    
    if (mustDownload) {
        for (const auto& src : SOURCES) {
            std::string html;
            if (Networking::downloadString(src.url, html, config.archive_cookie)) {
                parseHtmlArchive(html, src.zip_name, newEntries);
            } else {
                downloadSuccess = false;
                break;
            }
        }
        
        if (downloadSuccess && !newEntries.empty()) {
            entries = newEntries;
            sortCatalog();
            buildJumpTable();
            saveToCache();
            
            config.last_refresh = time(nullptr);
            Configuration::save(config);
            return true;
        } else {
            if (cacheExists) {
                mustDownload = false; // download failed, load from cache fallback
            } else {
                return false;
            }
        }
    }
    
    if (!mustDownload) {
        std::ifstream cacheFile("sdmc:/3ds/NitroShop/catalog.cache");
        if (cacheFile.is_open()) {
            std::stringstream buffer;
            buffer << cacheFile.rdbuf();
            std::string rawCache = buffer.str();
            cacheFile.close();
            
            parseCachedCatalog(rawCache);
            sortCatalog();
            buildJumpTable();
            return !entries.empty();
        }
    }
    
    return false;
}

bool Catalog::saveToCache() {
    FileSystem::createDirectory("sdmc:/3ds/NitroShop");
    std::ofstream file("sdmc:/3ds/NitroShop/catalog.cache");
    if (!file.is_open()) return false;
    
    file << "[\n";
    for (size_t i = 0; i < entries.size(); ++i) {
        file << "  {\n";
        file << "    \"raw_filename\": \"" << entries[i].raw_filename << "\",\n";
        file << "    \"url\": \"" << entries[i].url << "\",\n";
        file << "    \"title\": \"" << entries[i].title << "\",\n";
        file << "    \"id\": \"" << entries[i].id << "\",\n";
        file << "    \"region\": \"" << entries[i].region << "\",\n";
        file << "    \"size\": " << entries[i].size << "\n";
        file << "  }" << (i + 1 < entries.size() ? ",\n" : "\n");
    }
    file << "]\n";
    file.close();
    return true;
}

void Catalog::clearCache() {
    FileSystem::deleteFile("sdmc:/3ds/NitroShop/catalog.cache");
    entries.clear();
    for (int i = 0; i < 27; ++i) {
        jump_table[i] = -1;
    }
}
