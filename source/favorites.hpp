#pragma once

#include <string>
#include <vector>
#include "catalog.hpp"

class FavoritesManager {
private:
    std::vector<CatalogEntry> favorites;
    
    void parseFavoritesJson(const std::string& jsonContent);

public:
    FavoritesManager();
    
    bool load();
    bool save();
    
    bool isFavorite(const std::string& title) const;
    void addFavorite(const CatalogEntry& entry);
    void removeFavorite(const std::string& title);
    
    const std::vector<CatalogEntry>& getFavorites() const { return favorites; }
};
