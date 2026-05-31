#pragma once

#include <string>
#include <vector>
#include "configuration.hpp"

struct CatalogEntry {
    std::string raw_filename;
    std::string url;
    std::string title;
    std::string id;
    std::string region;
    uint64_t size = 0;
};

class Catalog {
private:
    std::vector<CatalogEntry> entries;
    int jump_table[27]; // 0 = # (numbers/symbols), 1-26 = A-Z

    void parseCachedCatalog(const std::string& jsonContent);
    void sortCatalog();
    bool saveToCache();

public:
    Catalog();
    
    // Loads catalog: checks cache first unless forceRefresh is true or cache is >24 hours old.
    // If downloading, it downloads HTML pages from all Archive.org ZIP viewer URLs, parses them, and caches them.
    bool load(bool forceRefresh, AppConfig& config);
    void clearCache();
    
    const std::vector<CatalogEntry>& getEntries() const { return entries; }
    void buildJumpTable();
    int getJumpIndex(char letter) const; // returns catalog index to jump to, or -1
};
