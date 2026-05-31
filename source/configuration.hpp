#pragma once

#include <string>

struct AppConfig {
    std::string download_dir;
    std::string catalog_url = "https://raw.githubusercontent.com/Universal-Team/db-meta/master/nds/titles.json"; // Default fallback url
    uint64_t last_refresh = 0;
    bool is_configured = false;
    std::string archive_cookie;
};

class Configuration {
public:
    static AppConfig load();
    static bool save(const AppConfig& config);
};
