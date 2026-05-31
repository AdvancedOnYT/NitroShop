#pragma once

#include <3ds.h>
#include <citro2d.h>
#include <string>
#include <vector>
#include "catalog.hpp"
#include "favorites.hpp"
#include "recent.hpp"
#include "download_manager.hpp"

enum class UIState {
    FIRST_LAUNCH_SETUP,
    LOADING_CATALOG,
    MAIN_STOREFRONT,
    GAME_DETAILS,
    DOWNLOADING,
    SETTINGS
};

enum class MainListType {
    ALL_GAMES,
    FAVORITES,
    RECENTS
};

class UI {
private:
    UIState state;
    MainListType list_type;
    
    C3D_RenderTarget* top_target;
    C3D_RenderTarget* bottom_target;
    
    C2D_TextBuf static_buf;
    C2D_TextBuf dynamic_buf;
    
    // Catalog list indexing
    int selected_idx;
    int scroll_offset;
    std::string search_query;
    std::vector<int> filtered_indices;
    
    // Directory browser indexing
    std::string current_dir;
    std::vector<std::string> dir_contents;
    int dir_selected_idx;
    int dir_scroll_offset;
    
    // Setup wizard indexing
    int setup_step;
    std::string setup_user;
    std::string setup_sig;
    bool show_chrome_instructions;
    
    // Settings index
    int settings_selected_idx;
    
    // Colors (HEX representation in comments)
    u32 color_bg;          // #080A0F (deep dark slate)
    u32 color_card;        // #121620 (dark card)
    u32 color_accent;      // #00E5FF (cyan glow)
    u32 color_accent_dim;  // #00363F (dark cyan)
    u32 color_text;        // #FFFFFF (white)
    u32 color_text_dim;    // #78909C (cool gray)
    u32 color_green;       // #2ECC71 (emerald)
    u32 color_red;         // #E74C3C (alizarin)
    u32 color_orange;      // #F39C12 (orange)

    void drawTopScreen(const Catalog& catalog, const FavoritesManager& favs, const RecentManager& recents, const DownloadManager& downloader, const AppConfig& config);
    void drawBottomScreen(const Catalog& catalog, const FavoritesManager& favs, const RecentManager& recents, const DownloadManager& downloader, const AppConfig& config);
    
    void drawButton(float x, float y, float w, float h, const std::string& text, u32 bg_color, u32 text_color, bool centered = true);
    void drawText(const std::string& text, float x, float y, float scale_x, float scale_y, u32 color);
    void drawTextCentered(const std::string& text, float y, float scale_x, float scale_y, u32 color, float screen_width = 400.0f);
    
    void updateDirContents();

public:
    void updateFilteredIndices(const Catalog& catalog, const FavoritesManager& favs, const RecentManager& recents);
    UI();
    ~UI();
    
    bool init();
    void exit();
    
    void handleInput(u32 kDown, u32 kHeld, const touchPosition& touch, const circlePosition& circle, 
                     Catalog& catalog, FavoritesManager& favs, RecentManager& recents, 
                     DownloadManager& downloader, AppConfig& config, bool& exit_app);
                     
    void render(const Catalog& catalog, const FavoritesManager& favs, const RecentManager& recents, const DownloadManager& downloader, const AppConfig& config);
    
    UIState getState() const { return state; }
    void setState(UIState s) { state = s; }
};
