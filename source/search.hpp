#pragma once

#include <string>
#include <vector>
#include "catalog.hpp"

namespace Search {
    // Launches 3DS keyboard to update the query
    bool showKeyboard(std::string& currentQuery);

    // Launches 3DS keyboard with custom hint and buffer size
    bool showCustomKeyboard(std::string& currentText, const std::string& hint, int maxLen = 256);

    // Filters the catalog entries using case-insensitive partial matching
    void filterCatalog(const std::vector<CatalogEntry>& catalog, const std::string& query, std::vector<int>& outFilteredIndices);
}
