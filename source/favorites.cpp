#include "favorites.hpp"
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

FavoritesManager::FavoritesManager() {}

void FavoritesManager::parseFavoritesJson(const std::string& jsonContent) {
    favorites.clear();
    
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
            favorites.push_back(entry);
        }
        
        pos = end + 1;
    }
}

bool FavoritesManager::load() {
    favorites.clear();
    if (!FileSystem::fileExists("sdmc:/3ds/NitroShop/favorites.json")) {
        return true; // No file is fine, starts empty
    }
    
    std::ifstream file("sdmc:/3ds/NitroShop/favorites.json");
    if (!file.is_open()) return false;
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();
    
    parseFavoritesJson(content);
    return true;
}

bool FavoritesManager::save() {
    FileSystem::createDirectory("sdmc:/3ds/NitroShop");
    std::ofstream file("sdmc:/3ds/NitroShop/favorites.json");
    if (!file.is_open()) return false;
    
    file << "[\n";
    for (size_t i = 0; i < favorites.size(); ++i) {
        file << "  {\n";
        file << "    \"title\": \"" << favorites[i].title << "\",\n";
        file << "    \"url\": \"" << favorites[i].url << "\",\n";
        file << "    \"size\": " << favorites[i].size << "\n";
        file << "  }" << (i + 1 < favorites.size() ? ",\n" : "\n");
    }
    file << "]\n";
    file.close();
    return true;
}

bool FavoritesManager::isFavorite(const std::string& title) const {
    for (const auto& entry : favorites) {
        if (entry.title == title) return true;
    }
    return false;
}

void FavoritesManager::addFavorite(const CatalogEntry& entry) {
    if (isFavorite(entry.title)) return;
    favorites.push_back(entry);
    save();
}

void FavoritesManager::removeFavorite(const std::string& title) {
    auto it = std::remove_if(favorites.begin(), favorites.end(), [&](const CatalogEntry& entry) {
        return entry.title == title;
    });
    if (it != favorites.end()) {
        favorites.erase(it, favorites.end());
        save();
    }
}
