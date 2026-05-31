#include "search.hpp"
#include <3ds.h>
#include <algorithm>
#include <cctype>

bool Search::showKeyboard(std::string& currentQuery) {
    SwkbdState swkbd;
    char mybuf[128];
    
    // Initialize standard keyboard on 3DS
    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, -1);
    swkbdSetHintText(&swkbd, "Search games...");
    
    if (!currentQuery.empty()) {
        swkbdSetInitialText(&swkbd, currentQuery.c_str());
    }
    
    SwkbdButton button = swkbdInputText(&swkbd, mybuf, sizeof(mybuf));
    if (button == SWKBD_BUTTON_CONFIRM) {
        currentQuery = mybuf;
        return true;
    }
    return false;
}

bool Search::showCustomKeyboard(std::string& currentText, const std::string& hint, int maxLen) {
    SwkbdState swkbd;
    std::vector<char> mybuf(maxLen);
    
    // Initialize standard keyboard on 3DS
    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, -1);
    swkbdSetHintText(&swkbd, hint.c_str());
    
    if (!currentText.empty()) {
        swkbdSetInitialText(&swkbd, currentText.c_str());
    }
    
    SwkbdButton button = swkbdInputText(&swkbd, mybuf.data(), maxLen);
    if (button == SWKBD_BUTTON_CONFIRM) {
        currentText = mybuf.data();
        return true;
    }
    return false;
}

void Search::filterCatalog(const std::vector<CatalogEntry>& catalog, const std::string& query, std::vector<int>& outFilteredIndices) {
    outFilteredIndices.clear();
    
    if (query.empty()) {
        outFilteredIndices.resize(catalog.size());
        for (size_t i = 0; i < catalog.size(); ++i) {
            outFilteredIndices[i] = static_cast<int>(i);
        }
        return;
    }
    
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), [](unsigned char c){ return std::tolower(c); });
    
    // Pre-reserve to avoid frequent re-allocation on 3DS heap
    outFilteredIndices.reserve(catalog.size() / 10);
    
    for (size_t i = 0; i < catalog.size(); ++i) {
        std::string lowerTitle = catalog[i].title;
        std::transform(lowerTitle.begin(), lowerTitle.end(), lowerTitle.begin(), [](unsigned char c){ return std::tolower(c); });
        
        if (lowerTitle.find(lowerQuery) != std::string::npos) {
            outFilteredIndices.push_back(static_cast<int>(i));
        }
    }
}
