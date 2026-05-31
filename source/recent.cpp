#include "recent.hpp"
#include "file_system.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

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

RecentManager::RecentManager() {}

void RecentManager::parseRecentJson(const std::string& jsonContent) {
    recents.clear();
    
    size_t pos = 0;
    while (true) {
        size_t start = jsonContent.find('{', pos);
        if (start == std::string::npos) break;
        
        size_t end = jsonContent.find('}', start);
        if (end == std::string::npos) break;
        
        std::string objStr = jsonContent.substr(start, end - start + 1);
        
        std::string title = extractJsonString(objStr, "title");
        std::string url = extractJsonString(objStr, "url");
        uint64_t size = extractJsonNumber(objStr, "size");
        
        if (!title.empty() && !url.empty()) {
            CatalogEntry entry;
            entry.title = title;
            entry.url = url;
            entry.size = size;
            recents.push_back(entry);
        }
        
        pos = end + 1;
    }
}

bool RecentManager::load() {
    recents.clear();
    if (!FileSystem::fileExists("sdmc:/3ds/NitroShop/recent.json")) {
        return true;
    }
    
    std::ifstream file("sdmc:/3ds/NitroShop/recent.json");
    if (!file.is_open()) return false;
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();
    
    parseRecentJson(content);
    return true;
}

bool RecentManager::save() {
    FileSystem::createDirectory("sdmc:/3ds/NitroShop");
    std::ofstream file("sdmc:/3ds/NitroShop/recent.json");
    if (!file.is_open()) return false;
    
    file << "[\n";
    for (size_t i = 0; i < recents.size(); ++i) {
        file << "  {\n";
        file << "    \"title\": \"" << recents[i].title << "\",\n";
        file << "    \"url\": \"" << recents[i].url << "\",\n";
        file << "    \"size\": " << recents[i].size << "\n";
        file << "  }" << (i + 1 < recents.size() ? ",\n" : "\n");
    }
    file << "]\n";
    file.close();
    return true;
}

void RecentManager::addRecent(const CatalogEntry& entry) {
    // Remove if already exists to move it to the front
    auto it = std::remove_if(recents.begin(), recents.end(), [&](const CatalogEntry& e) {
        return e.title == entry.title;
    });
    if (it != recents.end()) {
        recents.erase(it, recents.end());
    }
    
    // Insert at front
    recents.insert(recents.begin(), entry);
    
    // Keep maximum 20 entries
    if (recents.size() > 20) {
        recents.resize(20);
    }
    
    save();
}
