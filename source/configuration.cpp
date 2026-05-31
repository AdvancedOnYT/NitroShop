#include "configuration.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static void createDirectoryRecursive(const std::string& path) {
    size_t pos = 0;
    do {
        pos = path.find('/', pos + 1);
        std::string subpath = path.substr(0, pos);
        if (!subpath.empty() && subpath != "sdmc:") {
            mkdir(subpath.c_str(), 0777);
        }
    } while (pos != std::string::npos);
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

AppConfig Configuration::load() {
    AppConfig config;
    std::ifstream file("sdmc:/3ds/NitroShop/config.json");
    if (!file.is_open()) {
        config.is_configured = false;
        return config;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();
    
    config.download_dir = extractJsonString(content, "download_dir");
    std::string custom_url = extractJsonString(content, "catalog_url");
    if (!custom_url.empty()) {
        config.catalog_url = custom_url;
    }
    config.last_refresh = extractJsonNumber(content, "last_refresh");
    config.archive_cookie = extractJsonString(content, "archive_cookie");
    
    // Config is valid if download_dir and archive_cookie are set and not empty
    config.is_configured = !config.download_dir.empty() && !config.archive_cookie.empty();
    return config;
}

bool Configuration::save(const AppConfig& config) {
    createDirectoryRecursive("sdmc:/3ds/NitroShop");
    std::ofstream file("sdmc:/3ds/NitroShop/config.json");
    if (!file.is_open()) {
        return false;
    }
    
    file << "{\n";
    file << "  \"download_dir\": \"" << config.download_dir << "\",\n";
    file << "  \"catalog_url\": \"" << config.catalog_url << "\",\n";
    file << "  \"last_refresh\": " << config.last_refresh << ",\n";
    file << "  \"archive_cookie\": \"" << config.archive_cookie << "\"\n";
    file << "}\n";
    file.close();
    return true;
}
