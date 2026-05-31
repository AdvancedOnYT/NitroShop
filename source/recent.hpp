#pragma once

#include <string>
#include <vector>
#include "catalog.hpp"

class RecentManager {
private:
    std::vector<CatalogEntry> recents;
    
    void parseRecentJson(const std::string& jsonContent);

public:
    RecentManager();
    
    bool load();
    bool save();
    
    void addRecent(const CatalogEntry& entry);
    const std::vector<CatalogEntry>& getRecents() const { return recents; }
};
