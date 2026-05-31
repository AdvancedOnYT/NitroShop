#include "ui.hpp"
#include "search.hpp"
#include "file_system.hpp"
#include "web_server.hpp"
#include <iomanip>
#include <sstream>
#include <cmath>
#include <algorithm>

UI::UI()
    : state(UIState::LOADING_CATALOG), list_type(MainListType::ALL_GAMES),
      selected_idx(0), scroll_offset(0), search_query(""),
      current_dir("sdmc:/"), dir_selected_idx(0), dir_scroll_offset(0),
      setup_step(0), setup_user(""), setup_sig(""),
      show_chrome_instructions(false),
      settings_selected_idx(0) {
    
    // Define Dark Neon Palette colors
    color_bg         = C2D_Color32(10, 13, 20, 255);    // #0A0D14
    color_card       = C2D_Color32(21, 26, 38, 255);    // #151A26
    color_accent     = C2D_Color32(0, 229, 255, 255);   // #00E5FF
    color_accent_dim = C2D_Color32(0, 54, 63, 255);     // #00363F
    color_text       = C2D_Color32(255, 255, 255, 255); // #FFFFFF
    color_text_dim   = C2D_Color32(120, 144, 156, 255); // #78909C
    color_green      = C2D_Color32(46, 204, 113, 255);  // #2ECC71
    color_red        = C2D_Color32(231, 76, 60, 255);   // #E74C3C
    color_orange     = C2D_Color32(243, 156, 18, 255);  // #F39C12
}

UI::~UI() {
    exit();
}

bool UI::init() {
    // Init graphics engines
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    
    // Create render targets
    top_target = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    bottom_target = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    if (!top_target || !bottom_target) return false;
    
    // Allocate text buffers
    static_buf = C2D_TextBufNew(4096);
    dynamic_buf = C2D_TextBufNew(4096);
    if (!static_buf || !dynamic_buf) return false;
    
    // Scan root directory contents
    updateDirContents();
    
    return true;
}

void UI::exit() {
    WebServer::stop();
    if (static_buf) {
        C2D_TextBufDelete(static_buf);
        static_buf = nullptr;
    }
    if (dynamic_buf) {
        C2D_TextBufDelete(dynamic_buf);
        dynamic_buf = nullptr;
    }
    C2D_Fini();
    C3D_Fini();
    gfxExit();
}

void UI::updateDirContents() {
    dir_contents = FileSystem::getSubdirectories(current_dir);
    dir_selected_idx = 0;
    dir_scroll_offset = 0;
}

void UI::updateFilteredIndices(const Catalog& catalog, const FavoritesManager& favs, const RecentManager& recents) {
    if (list_type == MainListType::ALL_GAMES) {
        Search::filterCatalog(catalog.getEntries(), search_query, filtered_indices);
    } else if (list_type == MainListType::FAVORITES) {
        // Filter local favorites vector
        std::vector<CatalogEntry> favVector = favs.getFavorites();
        std::vector<int> tempIndices;
        Search::filterCatalog(favVector, search_query, tempIndices);
        
        // Match favorites back to primary catalog indices
        filtered_indices.clear();
        const auto& catalogEntries = catalog.getEntries();
        for (int idx : tempIndices) {
            std::string favTitle = favVector[idx].title;
            // Find index of favTitle in primary catalog
            for (size_t c = 0; c < catalogEntries.size(); ++c) {
                if (catalogEntries[c].title == favTitle) {
                    filtered_indices.push_back(static_cast<int>(c));
                    break;
                }
            }
        }
    } else if (list_type == MainListType::RECENTS) {
        // Filter local recents vector
        std::vector<CatalogEntry> recentVector = recents.getRecents();
        std::vector<int> tempIndices;
        Search::filterCatalog(recentVector, search_query, tempIndices);
        
        // Match recents back to primary catalog indices
        filtered_indices.clear();
        const auto& catalogEntries = catalog.getEntries();
        for (int idx : tempIndices) {
            std::string recTitle = recentVector[idx].title;
            for (size_t c = 0; c < catalogEntries.size(); ++c) {
                if (catalogEntries[c].title == recTitle) {
                    filtered_indices.push_back(static_cast<int>(c));
                    break;
                }
            }
        }
    }
    
    // Clamp selection bounds
    if (selected_idx >= static_cast<int>(filtered_indices.size())) {
        selected_idx = static_cast<int>(filtered_indices.size()) - 1;
    }
    if (selected_idx < 0) {
        selected_idx = 0;
    }
    if (scroll_offset > selected_idx) {
        scroll_offset = selected_idx;
    }
}

void UI::drawText(const std::string& text, float x, float y, float scale_x, float scale_y, u32 color) {
    if (text.empty()) return;
    C2D_Text txt;
    C2D_TextParse(&txt, dynamic_buf, text.c_str());
    C2D_TextOptimize(&txt);
    C2D_DrawText(&txt, C2D_WithColor, x, y, 0.5f, scale_x, scale_y, color);
}

void UI::drawTextCentered(const std::string& text, float y, float scale_x, float scale_y, u32 color, float screen_width) {
    if (text.empty()) return;
    C2D_Text txt;
    C2D_TextParse(&txt, dynamic_buf, text.c_str());
    C2D_TextOptimize(&txt);
    
    float textW = 0;
    float textH = 0;
    C2D_TextGetDimensions(&txt, scale_x, scale_y, &textW, &textH);
    
    float x = (screen_width - textW) / 2.0f;
    C2D_DrawText(&txt, C2D_WithColor, x, y, 0.5f, scale_x, scale_y, color);
}

void UI::drawButton(float x, float y, float w, float h, const std::string& text, u32 bg_color, u32 text_color, bool centered) {
    C2D_DrawRectSolid(x, y, 0.5f, w, h, bg_color);
    
    // Cyan glow borders
    C2D_DrawRectSolid(x, y, 0.5f, w, 1, color_accent_dim);
    C2D_DrawRectSolid(x, y + h - 1, 0.5f, w, 1, color_accent_dim);
    C2D_DrawRectSolid(x, y, 0.5f, 1, h, color_accent_dim);
    C2D_DrawRectSolid(x + w - 1, y, 0.5f, 1, h, color_accent_dim);
    
    C2D_Text txt;
    C2D_TextParse(&txt, dynamic_buf, text.c_str());
    C2D_TextOptimize(&txt);
    
    float textW = 0;
    float textH = 0;
    C2D_TextGetDimensions(&txt, 0.5f, 0.5f, &textW, &textH);
    
    float tx = x;
    float ty = y + (h - textH) / 2;
    if (centered) {
        tx += (w - textW) / 2;
    } else {
        tx += 6;
    }
    
    C2D_DrawText(&txt, C2D_WithColor, tx, ty, 0.5f, 0.5f, 0.5f, text_color);
}

void UI::render(const Catalog& catalog, const FavoritesManager& favs, const RecentManager& recents, const DownloadManager& downloader, const AppConfig& config) {
    // Clear dynamic text allocations
    C2D_TextBufClear(dynamic_buf);
    
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    
    // 1. Render Top Screen
    C2D_SceneBegin(top_target);
    C2D_TargetClear(top_target, color_bg);
    drawTopScreen(catalog, favs, recents, downloader, config);
    
    // 2. Render Bottom Screen
    C2D_SceneBegin(bottom_target);
    C2D_TargetClear(bottom_target, color_bg);
    drawBottomScreen(catalog, favs, recents, downloader, config);
    
    C3D_FrameEnd(0);
}

void UI::drawTopScreen(const Catalog& catalog, const FavoritesManager& favs, const RecentManager& recents, const DownloadManager& downloader, const AppConfig& config) {
    // Static header bar
    C2D_DrawRectSolid(0, 0, 0.5f, 400, 30, color_card);
    C2D_DrawRectSolid(0, 29, 0.5f, 400, 1, color_accent);
    
    if (state != UIState::FIRST_LAUNCH_SETUP && state != UIState::GAME_DETAILS && state != UIState::DOWNLOADING) {
        drawText("NitroShop", 10, 5, 0.6f, 0.6f, color_accent);
    }
    drawText("v1.0.1", 350, 8, 0.45f, 0.45f, color_text_dim);
    
    if (state == UIState::FIRST_LAUNCH_SETUP) {
        if (setup_step == 0) {
            drawTextCentered("Welcome to NitroShop Setup Wizard!", 5, 0.58f, 0.58f, color_accent);
            
            C2D_DrawRectSolid(15, 38, 0.5f, 370, 170, color_card);
            drawTextCentered("This wizard will guide you through configuring", 45, 0.44f, 0.44f, color_text);
            drawTextCentered("the application for the first time.", 60, 0.44f, 0.44f, color_text);
            
            drawTextCentered("Because Archive.org restricted NDS files,", 85, 0.44f, 0.44f, color_text_dim);
            drawTextCentered("you will select a destination folder and input", 100, 0.44f, 0.44f, color_text_dim);
            drawTextCentered("your Archive.org login session cookies.", 115, 0.44f, 0.44f, color_text_dim);
            
            drawTextCentered("This allows NitroShop to securely fetch the", 140, 0.44f, 0.44f, color_text_dim);
            drawTextCentered("catalog and download games directly to your SD.", 155, 0.44f, 0.44f, color_text_dim);
            
            drawTextCentered("Press A or tap Next Step to begin.", 185, 0.45f, 0.45f, color_green);
        } else if (setup_step == 1) {
            drawTextCentered("Step 1: Select Download Folder", 5, 0.58f, 0.58f, color_accent);
            
            // Show current browsing dir
            C2D_DrawRectSolid(10, 35, 0.5f, 380, 22, color_card);
            drawText("Dir: " + current_dir, 15, 38, 0.45f, 0.45f, color_text_dim);
            
            // Scrollable folders list
            float startY = 65;
            int maxVisible = 6;
            for (int i = 0; i < maxVisible; ++i) {
                int idx = dir_scroll_offset + i;
                if (idx >= static_cast<int>(dir_contents.size())) break;
                
                float itemY = startY + i * 26;
                bool selected = (idx == dir_selected_idx);
                
                u32 bgCol = selected ? color_accent_dim : color_card;
                u32 textCol = selected ? color_accent : color_text;
                
                C2D_DrawRectSolid(10, itemY, 0.5f, 380, 24, bgCol);
                if (selected) {
                    C2D_DrawRectSolid(10, itemY, 0.5f, 2, 24, color_accent);
                }
                
                drawText("/ " + dir_contents[idx], 18, itemY + 4, 0.48f, 0.48f, textCol);
            }
            
            if (dir_contents.empty()) {
                drawText("(No subdirectories found)", 100, 120, 0.5f, 0.5f, color_text_dim);
            }
        } else if (setup_step == 2) {
            if (show_chrome_instructions) {
                drawTextCentered("Chrome Cookie Guide", 5, 0.58f, 0.58f, color_accent);
                
                C2D_DrawRectSolid(15, 38, 0.5f, 370, 170, color_card);
                
                drawTextCentered("1. Log in to the archive.org website.", 45, 0.44f, 0.44f, color_text);
                drawTextCentered("2. Open Developer Tools (F12 or inspect).", 70, 0.44f, 0.44f, color_text);
                drawTextCentered("3. Look for and click 'Application'.", 95, 0.44f, 0.44f, color_text);
                drawTextCentered("4. Click the 'Cookies' dropdown menu.", 120, 0.44f, 0.44f, color_text);
                drawTextCentered("5. Click the archive.org URL to find them.", 145, 0.44f, 0.44f, color_text);
                
                drawTextCentered("Press X / tap Close to return.", 185, 0.45f, 0.45f, color_orange);
            } else {
                drawTextCentered("Step 2: Get Archive.org Cookies", 5, 0.58f, 0.58f, color_accent);
                
                C2D_DrawRectSolid(15, 38, 0.5f, 370, 170, color_card);
                drawTextCentered("1. On a computer or phone, log in to archive.org.", 45, 0.44f, 0.44f, color_text);
                drawTextCentered("2. Open Developer Tools (F12) and go to Cookies.", 65, 0.44f, 0.44f, color_text);
                drawTextCentered("3. Look up and copy the values for these keys:", 85, 0.44f, 0.44f, color_text);
                
                drawTextCentered("logged-in-user (usually your email address)", 110, 0.44f, 0.44f, color_accent);
                drawTextCentered("logged-in-sig (a long session token)", 130, 0.44f, 0.44f, color_accent);
                
                drawTextCentered("In the next step, you will type these in.", 160, 0.44f, 0.44f, color_text_dim);
                drawTextCentered("Press A or tap Next Step to proceed.", 185, 0.45f, 0.45f, color_green);
            }
        } else if (setup_step == 3) {
            drawTextCentered("Step 3: Enter Credentials", 5, 0.58f, 0.58f, color_accent);
            
            C2D_DrawRectSolid(15, 38, 0.5f, 370, 170, color_card);
            
            drawTextCentered("logged-in-user:", 45, 0.44f, 0.44f, color_text_dim);
            std::string dispUser = setup_user;
            if (dispUser.empty()) dispUser = "(Tap bottom field to enter email)";
            if (dispUser.size() > 42) dispUser = dispUser.substr(0, 39) + "...";
            drawTextCentered(dispUser, 60, 0.44f, 0.44f, setup_user.empty() ? color_orange : color_text);
            
            drawTextCentered("logged-in-sig:", 95, 0.44f, 0.44f, color_text_dim);
            std::string dispSig = setup_sig;
            if (dispSig.empty()) dispSig = "(Tap bottom field to enter signature)";
            if (dispSig.size() > 42) dispSig = dispSig.substr(0, 39) + "...";
            drawTextCentered(dispSig, 110, 0.44f, 0.44f, setup_sig.empty() ? color_orange : color_text);
            
            if (WebServer::isRunning()) {
                drawTextCentered("Web Setup: http://" + WebServer::getLocalIP() + ":8080", 130, 0.42f, 0.42f, color_accent);
                drawTextCentered("Open this URL on your phone/PC to paste cookies.", 148, 0.40f, 0.40f, color_text_dim);
                drawTextCentered("Or edit manually on the bottom screen.", 166, 0.40f, 0.40f, color_text_dim);
                if (!setup_user.empty() && !setup_sig.empty()) {
                    drawTextCentered("Press A or tap Finish to proceed.", 184, 0.42f, 0.42f, color_green);
                }
            } else {
                if (!setup_user.empty() && !setup_sig.empty()) {
                    drawTextCentered("Configuration complete! Press A to finish.", 165, 0.45f, 0.45f, color_green);
                } else {
                    drawTextCentered("Please fill out both fields on the bottom screen.", 165, 0.42f, 0.42f, color_orange);
                }
            }
        }
    } else if (state == UIState::LOADING_CATALOG) {
        drawTextCentered("Loading catalog...", 85, 0.6f, 0.6f, color_text);
        drawTextCentered("Downloading ZIP tables from Archive.org", 115, 0.45f, 0.45f, color_text_dim);
        drawTextCentered("This may take a few minutes on first launch.", 140, 0.42f, 0.42f, color_orange);
        drawTextCentered("Please do not close the application.", 160, 0.42f, 0.42f, color_text_dim);
        
    } else if (state == UIState::MAIN_STOREFRONT) {
        // Subheader showing filter type
        std::string modeStr = "All Games";
        if (list_type == MainListType::FAVORITES) modeStr = "Favorites";
        else if (list_type == MainListType::RECENTS) modeStr = "Recents (Last 20)";
        
        if (!search_query.empty()) {
            modeStr += " - Search: \"" + search_query + "\"";
        }
        
        drawTextCentered(modeStr, 5, 0.6f, 0.6f, color_text, 400.0f);
        
        // Show scroll list
        float startY = 35;
        int maxVisible = 7;
        for (int i = 0; i < maxVisible; ++i) {
            int idx = scroll_offset + i;
            if (idx >= static_cast<int>(filtered_indices.size())) break;
            
            int catalogIdx = filtered_indices[idx];
            const auto& entry = catalog.getEntries()[catalogIdx];
            
            float itemY = startY + i * 26;
            bool selected = (idx == selected_idx);
            
            u32 bgCol = selected ? color_accent_dim : color_card;
            u32 textCol = selected ? color_accent : color_text;
            
            C2D_DrawRectSolid(10, itemY, 0.5f, 380, 24, bgCol);
            if (selected) {
                C2D_DrawRectSolid(10, itemY, 0.5f, 2, 24, color_accent);
            }
            
            // Draw title text (truncated if too long to prevent overflowing screen)
            std::string dispTitle = entry.title;
            if (dispTitle.size() > 42) {
                dispTitle = dispTitle.substr(0, 39) + "...";
            }
            
            drawText(dispTitle, 15, itemY + 4, 0.48f, 0.48f, textCol);
            
            // Draw Star if favorite
            if (favs.isFavorite(entry.title)) {
                drawText("[Fav]", 350, itemY + 4, 0.45f, 0.45f, color_orange);
            }
        }
        
        if (filtered_indices.empty()) {
            drawText("No items match query.", 120, 110, 0.5f, 0.5f, color_text_dim);
        }
        
        // Scroll statistics footer
        C2D_DrawRectSolid(0, 222, 0.5f, 400, 18, color_card);
        std::stringstream ss;
        ss << "Index: " << (filtered_indices.empty() ? 0 : selected_idx + 1) << " / " << filtered_indices.size();
        drawText(ss.str(), 10, 224, 0.42f, 0.42f, color_text_dim);
        
        drawText("D-Pad: Scroll | A: Info | Y: Search | B: Settings", 115, 224, 0.40f, 0.40f, color_text_dim);
        
    } else if (state == UIState::GAME_DETAILS) {
        drawTextCentered("Game Details", 5, 0.6f, 0.6f, color_text, 400.0f);
        
        if (selected_idx < static_cast<int>(filtered_indices.size())) {
            int catIdx = filtered_indices[selected_idx];
            const auto& entry = catalog.getEntries()[catIdx];
            
            // Info Cards
            C2D_DrawRectSolid(15, 38, 0.5f, 370, 75, color_card);
            C2D_DrawRectSolid(15, 38, 0.5f, 370, 1, color_accent_dim);
            
            // Center title text
            std::string mainTitle = entry.title;
            if (mainTitle.size() > 42) {
                drawTextCentered(mainTitle.substr(0, 39) + "...", 45, 0.45f, 0.45f, color_text, 400.0f);
            } else {
                drawTextCentered(mainTitle, 45, 0.45f, 0.45f, color_text, 400.0f);
            }
            
            // Center ID and Region text on a single line
            std::string infoStr = "ID: " + entry.id + "   |   Region: " + entry.region;
            drawTextCentered(infoStr, 76, 0.45f, 0.45f, color_accent, 400.0f);
            
            // URL Info (centered)
            C2D_DrawRectSolid(15, 120, 0.5f, 370, 48, color_card);
            drawTextCentered("Download URL", 124, 0.45f, 0.45f, color_text_dim, 400.0f);
            
            std::string dispUrl = entry.url;
            if (dispUrl.size() > 50) {
                dispUrl = dispUrl.substr(0, 47) + "...";
            }
            drawTextCentered(dispUrl, 142, 0.4f, 0.4f, color_text_dim, 400.0f);
            
            std::string targetFile = entry.raw_filename;
            std::string finalPath = config.download_dir;
            if (finalPath.back() != '/') finalPath += "/";
            std::string ndsPath = finalPath + targetFile;
            
            bool completeExists = FileSystem::fileExists(ndsPath);
            
            if (completeExists) {
                drawTextCentered("Game downloaded completely. Press A to redownload.", 185, 0.45f, 0.45f, color_green, 400.0f);
            } else {
                drawTextCentered("Ready to download directly to SD Card.", 185, 0.45f, 0.45f, color_text_dim, 400.0f);
            }
        }
        
    } else if (state == UIState::DOWNLOADING) {
        drawTextCentered("Downloading Game", 5, 0.6f, 0.6f, color_text);
        
        CatalogEntry entry = downloader.getCurrentEntry();
        std::string rawTitle = entry.title;
        if (rawTitle.size() > 40) rawTitle = rawTitle.substr(0, 37) + "...";
        drawTextCentered(rawTitle, 45, 0.5f, 0.5f, color_accent);
        
        // ROM Size display
        std::stringstream romSizeSS;
        romSizeSS << std::fixed << std::setprecision(1);
        if (entry.size > 0) {
            double romSizeMB = entry.size / (1024.0 * 1024.0);
            romSizeSS << "ROM Size: " << romSizeMB << " MB";
        } else {
            romSizeSS << "ROM Size: Unknown";
        }
        drawTextCentered(romSizeSS.str(), 63, 0.45f, 0.45f, color_text_dim);
        
        // Progress Bar (centered horizontally)
        float barW = 360;
        float barH = 20;
        float barX = (400 - barW) / 2.0f;
        float barY = 88;
        
        C2D_DrawRectSolid(barX, barY, 0.5f, barW, barH, color_card);
        
        uint64_t dl = downloader.getDownloadedBytes();
        uint64_t tot = downloader.getTotalBytes();
        double ratio = (tot > 0) ? (static_cast<double>(dl) / tot) : 0.0;
        if (ratio > 1.0) ratio = 1.0;
        
        if (tot > 0) {
            C2D_DrawRectSolid(barX, barY, 0.5f, barW * ratio, barH, color_accent);
        } else {
            // Unknown total: show indeterminate pulsing bar
            float pulseW = barW * 0.3f; // 108
            // Make it pulse across the whole bar area
            float pulseRange = barW - pulseW; // 252
            // Use a smoother pulse: sin wave with safe modulo to prevent float precision lock-up
            float time = (float)(osGetTime() % 6283) / 1000.0f; // 6283 ms represents ~2*pi seconds
            float pulseOffset = (std::sin(time * 2.0f) + 1.0f) / 2.0f * pulseRange;
            C2D_DrawRectSolid(barX + pulseOffset, barY, 0.5f, pulseW, barH, color_accent);
        }
        
        // Borders around bar
        C2D_DrawRectSolid(barX, barY, 0.5f, barW, 1, color_accent_dim);
        C2D_DrawRectSolid(barX, barY + barH - 1, 0.5f, barW, 1, color_accent_dim);
        C2D_DrawRectSolid(barX, barY, 0.5f, 1, barH, color_accent_dim);
        C2D_DrawRectSolid(barX + barW - 1, barY, 0.5f, 1, barH, color_accent_dim);
        
        // Stats text
        double dlMB = dl / (1024.0 * 1024.0);
        
        std::stringstream stats;
        stats << std::fixed << std::setprecision(1);
        if (tot > 0) {
            double totMB = tot / (1024.0 * 1024.0);
            stats << dlMB << " MB / " << totMB << " MB  (" << (int)(ratio * 100) << "%)";
        } else {
            stats << dlMB << " MB downloaded";
        }
        drawTextCentered(stats.str(), 118, 0.48f, 0.48f, color_text);
        
        // Download Speed calculation
        double speed = downloader.getDownloadSpeed(); // bytes/sec
        std::stringstream speedSS;
        speedSS << std::fixed << std::setprecision(1);
        if (speed >= 1024 * 1024) {
            speedSS << (speed / (1024.0 * 1024.0)) << " MB/s";
        } else if (speed > 0) {
            speedSS << (speed / 1024.0) << " KB/s";
        } else {
            speedSS << "-- KB/s";
        }
        
        // ETA calculation
        double etaSeconds = downloader.getETA();
        std::string etaStr;
        if (tot > 0 && etaSeconds > 0 && speed > 0) {
            etaStr = "ETA: ";
            if (etaSeconds > 3600) {
                int hrs = (int)(etaSeconds / 3600);
                int mins = (int)((etaSeconds - hrs * 3600) / 60);
                etaStr += std::to_string(hrs) + "h " + std::to_string(mins) + "m";
            } else if (etaSeconds > 60) {
                int mins = (int)(etaSeconds / 60);
                int secs = (int)etaSeconds % 60;
                etaStr += std::to_string(mins) + "m " + std::to_string(secs) + "s";
            } else {
                etaStr += std::to_string((int)etaSeconds) + "s";
            }
        } else {
            etaStr = "ETA: Calculating...";
        }
        
        // Combined speed + ETA line centered
        std::string speedEtaLine = "Speed: " + speedSS.str() + "   |   " + etaStr;
        drawTextCentered(speedEtaLine, 143, 0.45f, 0.45f, color_text_dim);
        
        // Bottom instructions or error
        if (downloader.getStatus() == DownloadStatus::ERROR) {
            std::string errMsg = downloader.getErrorMessage();
            if (errMsg.size() > 50) errMsg = errMsg.substr(0, 47) + "...";
            drawTextCentered(errMsg, 170, 0.45f, 0.45f, color_red, 400.0f);
            drawTextCentered("Press A or B to return.", 185, 0.45f, 0.45f, color_text, 400.0f);
        } else {
            drawTextCentered("Streaming download directly to SD card.", 170, 0.45f, 0.45f, color_text_dim);
            drawTextCentered("Do not close the console or exit wireless range.", 185, 0.45f, 0.45f, color_text_dim);
        }
        
    } else if (state == UIState::SETTINGS) {
        drawTextCentered("Settings", 5, 0.6f, 0.6f, color_text, 400.0f);
        
        std::vector<std::pair<std::string, std::string>> options = {
            {"Change Download Directory", "Current: " + config.download_dir},
            {"Change Archive.org Cookies", "Configure your login session credentials"},
            {"Refresh Catalog Now", "Downloads and rebuilds local database"},
            {"Clear Cached Catalog", "Deletes catalog cache file on SD"},
            {"App Info / Version", "NitroShop by Advanced / v1.0.1"},
            {"Exit NitroShop", "Return to Homebrew Launcher / Home Menu"}
        };
        
        float startY = 32;
        for (size_t i = 0; i < options.size(); ++i) {
            float itemY = startY + i * 30;
            bool selected = (static_cast<int>(i) == settings_selected_idx);
            
            u32 bgCol = selected ? color_accent_dim : color_card;
            u32 textCol = selected ? color_accent : color_text;
            
            C2D_DrawRectSolid(10, itemY, 0.5f, 380, 26, bgCol);
            if (selected) {
                C2D_DrawRectSolid(10, itemY, 0.5f, 2, 26, color_accent);
            }
            
            drawText(options[i].first, 18, itemY + 2, 0.44f, 0.44f, textCol);
            drawText(options[i].second, 18, itemY + 14, 0.36f, 0.36f, color_text_dim);
        }
    }
}

void UI::drawBottomScreen(const Catalog& catalog, const FavoritesManager& favs, const RecentManager& recents, const DownloadManager& downloader, const AppConfig& config) {
    if (state == UIState::FIRST_LAUNCH_SETUP) {
        if (setup_step == 0) {
            drawTextCentered("Welcome Controller", 10, 0.55f, 0.55f, color_accent, 320.0f);
            drawButton(20, 60, 280, 42, "Next Step (A)", color_accent_dim, color_accent);
            drawButton(20, 120, 280, 42, "Exit Application", color_card, color_text);
        } else if (setup_step == 1) {
            drawTextCentered("Setup Directory Controller", 10, 0.55f, 0.55f, color_accent, 320.0f);
            drawButton(20, 50, 280, 42, "Select Current Folder", color_accent_dim, color_accent);
            drawButton(20, 105, 280, 42, "Create New Folder", color_card, color_text);
            drawButton(20, 160, 280, 42, "Back (B)", color_card, color_text);
        } else if (setup_step == 2) {
            drawTextCentered("Instructions Controller", 10, 0.55f, 0.55f, color_accent, 320.0f);
            if (show_chrome_instructions) {
                drawButton(20, 60, 280, 42, "Close Chrome Guide (X)", color_red, color_text);
            } else {
                drawButton(20, 60, 280, 42, "Chrome Guide (X)", color_accent_dim, color_accent);
            }
            drawButton(20, 115, 280, 38, "Next Step (A)", color_green, color_bg);
            drawButton(20, 165, 280, 38, "Back (B)", color_card, color_text);
        } else if (setup_step == 3) {
            drawTextCentered("Credentials Input Controller", 10, 0.55f, 0.55f, color_accent, 320.0f);
            drawButton(20, 45, 280, 38, "Edit User (Email)", color_card, color_text);
            drawButton(20, 95, 280, 38, "Edit Signature (Sig)", color_card, color_text);
            
            bool bothSet = !setup_user.empty() && !setup_sig.empty();
            u32 finBg = bothSet ? color_green : color_accent_dim;
            u32 finTx = bothSet ? color_bg : color_text_dim;
            drawButton(20, 145, 135, 38, "Finish (A)", finBg, finTx);
            drawButton(165, 145, 135, 38, "Back (B)", color_card, color_text);
            
            if (WebServer::isRunning()) {
                drawText("Local web portal listening on port 8080...", 20, 195, 0.38f, 0.38f, color_text_dim);
            }
        }
    } else if (state == UIState::LOADING_CATALOG) {
        drawTextCentered("Console status check: Connected", 70, 0.5f, 0.5f, color_green, 320.0f);
        drawTextCentered("Preparing SD filesystem structures...", 110, 0.45f, 0.45f, color_text_dim, 320.0f);
        drawTextCentered("This may take a few minutes.", 140, 0.42f, 0.42f, color_orange, 320.0f);
        drawTextCentered("Please stand by.", 165, 0.45f, 0.45f, color_text_dim, 320.0f);
        
    } else if (state == UIState::MAIN_STOREFRONT) {
        // Draw touch jump-to-letter grid
        // 3 rows, 9 columns = 27 slots
        for (int i = 0; i < 27; ++i) {
            int row = i / 9;
            int col = i % 9;
            float bx = 10 + col * 34;
            float by = 15 + row * 34;
            float bw = 30;
            float bh = 30;
            
            char keyChar = '#';
            if (i > 0) {
                keyChar = 'A' + (i - 1);
            }
            
            std::string keyStr(1, keyChar);
            if (i == 0) keyStr = "#";
            
            // Check if jump index exists, if not gray it out
            int jumpTarget = catalog.getJumpIndex(keyChar);
            u32 keyTextCol = (jumpTarget != -1) ? color_text : color_text_dim;
            u32 keyBgCol = color_card;
            
            drawButton(bx, by, bw, bh, keyStr, keyBgCol, keyTextCol);
        }
        
        // Navigation Buttons under the grid
        // Row 1 (y = 125)
        drawButton(10, 125, 145, 34, "Search (Y)", color_accent_dim, color_accent);
        drawButton(165, 125, 145, 34, "Settings (Sel)", color_card, color_text);
        
        // Row 2 (y = 168)
        u32 allBg = (list_type == MainListType::ALL_GAMES) ? color_accent_dim : color_card;
        u32 allTx = (list_type == MainListType::ALL_GAMES) ? color_accent : color_text;
        u32 favBg = (list_type == MainListType::FAVORITES) ? color_accent_dim : color_card;
        u32 favTx = (list_type == MainListType::FAVORITES) ? color_accent : color_text;
        u32 recBg = (list_type == MainListType::RECENTS) ? color_accent_dim : color_card;
        u32 recTx = (list_type == MainListType::RECENTS) ? color_accent : color_text;
        
        drawButton(10, 168, 95, 34, "All Games", allBg, allTx);
        drawButton(112, 168, 95, 34, "Favorites", favBg, favTx);
        drawButton(215, 168, 95, 34, "Recents", recBg, recTx);
        
        // Quick stats line
        std::stringstream qss;
        qss << "Showing " << filtered_indices.size() << " entry items.";
        drawText(qss.str(), 15, 215, 0.42f, 0.42f, color_text_dim);
        
        // Clear search instruction
        if (!search_query.empty()) {
            drawText("Press B to clear query.", 175, 215, 0.42f, 0.42f, color_orange);
        }
        
    } else if (state == UIState::GAME_DETAILS) {
        drawTextCentered("Controller Actions", 10, 0.55f, 0.55f, color_accent, 320.0f);
        
        // Action box (now taller and centered vertically)
        C2D_DrawRectSolid(10, 30, 0.5f, 300, 195, color_card);
        
        // Large Touch buttons inside the Action box card
        int catIdx = filtered_indices[selected_idx];
        const auto& entry = catalog.getEntries()[catIdx];
        bool isFav = favs.isFavorite(entry.title);
        
        drawButton(20, 40, 280, 36, "Download Game (A)", color_green, color_bg);
        drawButton(20, 86, 280, 36, isFav ? "Remove Favorite (X)" : "Add Favorite (X)", color_orange, color_bg);
        drawButton(20, 132, 280, 36, "Return to Catalog (B)", color_accent_dim, color_accent);
        
        drawTextCentered("TWiLight Menu++ ROM folder compatible.", 188, 0.42f, 0.42f, color_text_dim, 320.0f);
        
    } else if (state == UIState::DOWNLOADING) {
        if (downloader.getStatus() == DownloadStatus::ERROR) {
            drawTextCentered("Download Error", 15, 0.55f, 0.55f, color_red, 320.0f);
            drawButton(20, 80, 280, 50, "Dismiss Error (A/B)", color_card, color_text);
        } else {
            drawTextCentered("Download Active", 15, 0.55f, 0.55f, color_accent, 320.0f);
            drawButton(20, 80, 280, 50, "CANCEL DOWNLOAD (B)", color_red, color_text);
            drawTextCentered("Downloading directly to SD card.", 155, 0.42f, 0.42f, color_text_dim, 320.0f);
            drawTextCentered("Press B to cancel the download.", 175, 0.42f, 0.42f, color_text_dim, 320.0f);
        }
        
    } else if (state == UIState::SETTINGS) {
        drawText("Settings Info Panel", 15, 15, 0.55f, 0.55f, color_accent);
        
        C2D_DrawRectSolid(10, 45, 0.5f, 300, 140, color_card);
        
        drawText("Use D-Pad Up/Down to navigate settings", 25, 60, 0.45f, 0.45f, color_text);
        drawText("Press A to select / change options", 25, 80, 0.45f, 0.45f, color_text);
        drawText("Press B to return to Storefront", 25, 100, 0.45f, 0.45f, color_text);
        
        drawButton(20, 195, 280, 36, "Back to storefront (B)", color_card, color_text);
    }
}

void UI::handleInput(u32 kDown, u32 kHeld, const touchPosition& touch, const circlePosition& circle, 
                     Catalog& catalog, FavoritesManager& favs, RecentManager& recents, 
                     DownloadManager& downloader, AppConfig& config, bool& exit_app) {
    
    // Poll local web portal if running
    if (state == UIState::FIRST_LAUNCH_SETUP && setup_step == 3) {
        if (WebServer::poll(setup_user, setup_sig)) {
            config.archive_cookie = "logged-in-user=" + setup_user + "; logged-in-sig=" + setup_sig;
            config.is_configured = true;
            Configuration::save(config);
            WebServer::stop();
            state = UIState::LOADING_CATALOG;
        }
    }

    if (state == UIState::FIRST_LAUNCH_SETUP) {
        if (setup_step == 0) {
            if (kDown & KEY_A) {
                setup_step = 1;
                updateDirContents();
            }
            if (kDown & KEY_TOUCH) {
                u16 px = touch.px;
                u16 py = touch.py;
                if (px >= 20 && px <= 300 && py >= 60 && py <= 102) {
                    setup_step = 1;
                    updateDirContents();
                } else if (px >= 20 && px <= 300 && py >= 120 && py <= 162) {
                    exit_app = true;
                }
            }
        } else if (setup_step == 1) {
            // Folder Navigation Inputs
            if (kDown & KEY_DUP) {
                dir_selected_idx--;
                if (dir_selected_idx < 0) {
                    dir_selected_idx = static_cast<int>(dir_contents.size()) - 1;
                }
            }
            if (kDown & KEY_DDOWN) {
                dir_selected_idx++;
                if (dir_selected_idx >= static_cast<int>(dir_contents.size())) {
                    dir_selected_idx = 0;
                }
            }
            
            // Scroll adjustment
            if (dir_selected_idx < dir_scroll_offset) {
                dir_scroll_offset = dir_selected_idx;
            }
            if (dir_selected_idx >= dir_scroll_offset + 6) {
                dir_scroll_offset = dir_selected_idx - 5;
            }
            
            if (kDown & KEY_A) {
                if (dir_selected_idx < static_cast<int>(dir_contents.size())) {
                    std::string targetSub = dir_contents[dir_selected_idx];
                    if (current_dir.back() != '/') current_dir += "/";
                    current_dir += targetSub;
                    updateDirContents();
                }
            }
            
            if (kDown & KEY_B) {
                // Move up directory tree, but if at root, go back to step 0
                if (current_dir != "sdmc:" && current_dir != "sdmc:/") {
                    size_t slash = current_dir.find_last_of('/');
                    if (slash != std::string::npos && slash > 5) {
                        current_dir = current_dir.substr(0, slash);
                    } else {
                        current_dir = "sdmc:/";
                    }
                    updateDirContents();
                } else {
                    setup_step = 0;
                }
            }
            
            // Touch events
            if (kDown & KEY_TOUCH) {
                u16 px = touch.px;
                u16 py = touch.py;
                
                // "Select Current Folder" button
                if (px >= 20 && px <= 300 && py >= 50 && py <= 92) {
                    config.download_dir = current_dir;
                    setup_step = 2;
                    show_chrome_instructions = false;
                }
                // "Create New Folder" button
                else if (px >= 20 && px <= 300 && py >= 105 && py <= 147) {
                    std::string newFolderName;
                    if (Search::showCustomKeyboard(newFolderName, "Enter folder name...")) {
                        if (!newFolderName.empty()) {
                            std::string creationPath = current_dir;
                            if (creationPath.back() != '/') creationPath += "/";
                            creationPath += newFolderName;
                            FileSystem::createDirectory(creationPath);
                            updateDirContents();
                        }
                    }
                }
                // "Cancel & Go Back" button
                else if (px >= 20 && px <= 300 && py >= 160 && py <= 202) {
                    setup_step = 0;
                }
            }
        } else if (setup_step == 2) {
            if (kDown & KEY_X) {
                show_chrome_instructions = !show_chrome_instructions;
            }
            if (!show_chrome_instructions) {
                if (kDown & KEY_A) {
                    setup_step = 3;
                    WebServer::start();
                }
                if (kDown & KEY_B) {
                    setup_step = 1;
                }
            }
            if (kDown & KEY_TOUCH) {
                u16 px = touch.px;
                u16 py = touch.py;
                if (px >= 20 && px <= 300 && py >= 60 && py <= 102) {
                    show_chrome_instructions = !show_chrome_instructions;
                }
                if (!show_chrome_instructions) {
                    if (px >= 20 && px <= 300 && py >= 115 && py <= 153) {
                        setup_step = 3;
                        WebServer::start();
                    } else if (px >= 20 && px <= 300 && py >= 165 && py <= 203) {
                        setup_step = 1;
                    }
                }
            }
        } else if (setup_step == 3) {
            if (kDown & KEY_B) {
                WebServer::stop();
                if (config.is_configured) {
                    state = UIState::SETTINGS;
                } else {
                    setup_step = 2;
                    show_chrome_instructions = false;
                }
            }
            
            bool bothSet = !setup_user.empty() && !setup_sig.empty();
            if ((kDown & KEY_A) && bothSet) {
                // Save and proceed!
                config.archive_cookie = "logged-in-user=" + setup_user + "; logged-in-sig=" + setup_sig;
                config.is_configured = true;
                Configuration::save(config);
                WebServer::stop();
                state = UIState::LOADING_CATALOG;
            }
            
            if (kDown & KEY_TOUCH) {
                u16 px = touch.px;
                u16 py = touch.py;
                
                // Edit user (email)
                if (px >= 20 && px <= 300 && py >= 45 && py <= 83) {
                    Search::showCustomKeyboard(setup_user, "Enter logged-in-user (email)...", 128);
                }
                // Edit sig (signature)
                else if (px >= 20 && px <= 300 && py >= 95 && py <= 133) {
                    Search::showCustomKeyboard(setup_sig, "Enter logged-in-sig...", 256);
                }
                // Finish
                else if (px >= 20 && px <= 155 && py >= 145 && py <= 183 && bothSet) {
                    config.archive_cookie = "logged-in-user=" + setup_user + "; logged-in-sig=" + setup_sig;
                    config.is_configured = true;
                    Configuration::save(config);
                    WebServer::stop();
                    state = UIState::LOADING_CATALOG;
                }
                // Back
                else if (px >= 165 && px <= 300 && py >= 145 && py <= 183) {
                    WebServer::stop();
                    if (config.is_configured) {
                        state = UIState::SETTINGS;
                    } else {
                        setup_step = 2;
                        show_chrome_instructions = false;
                    }
                }
            }
        }
    } else if (state == UIState::MAIN_STOREFRONT) {
        int listSize = static_cast<int>(filtered_indices.size());
        
        // Circle pad Scroll
        if (std::abs(circle.dy) > 30) {
            static u64 lastScroll = 0;
            u64 now = osGetTime();
            if (now - lastScroll > 150) { // scrolling speed limiter
                if (circle.dy > 0) {
                    selected_idx--;
                } else {
                    selected_idx++;
                }
                lastScroll = now;
            }
        }
        
        // Key navigation
        if (kDown & KEY_DUP) {
            selected_idx--;
        }
        if (kDown & KEY_DDOWN) {
            selected_idx++;
        }
        
        // Clamp selection bounds
        if (selected_idx < 0) {
            selected_idx = listSize - 1; // wrapping
        }
        if (selected_idx >= listSize) {
            selected_idx = 0; // wrapping
        }
        
        // Page up/down
        if (kDown & KEY_L) {
            selected_idx -= 7;
            if (selected_idx < 0) selected_idx = 0;
        }
        if (kDown & KEY_R) {
            selected_idx += 7;
            if (selected_idx >= listSize) selected_idx = listSize - 1;
        }
        
        // Scroll alignment
        if (selected_idx < scroll_offset) {
            scroll_offset = selected_idx;
        }
        if (selected_idx >= scroll_offset + 7) {
            scroll_offset = selected_idx - 6;
        }
        
        // Action keys
        if ((kDown & KEY_A) && listSize > 0) {
            state = UIState::GAME_DETAILS;
        }
        
        if (kDown & KEY_Y) {
            std::string tempQuery = search_query;
            if (Search::showKeyboard(tempQuery)) {
                search_query = tempQuery;
                selected_idx = 0;
                scroll_offset = 0;
                updateFilteredIndices(catalog, favs, recents);
            }
        }
        
        if (kDown & KEY_B) {
            if (!search_query.empty()) {
                search_query.clear();
                selected_idx = 0;
                scroll_offset = 0;
                updateFilteredIndices(catalog, favs, recents);
            } else {
                state = UIState::SETTINGS;
            }
        }
        
        if (kDown & KEY_SELECT) {
            state = UIState::SETTINGS;
        }
        
        if ((kDown & KEY_X) && listSize > 0) {
            int catIdx = filtered_indices[selected_idx];
            const auto& entry = catalog.getEntries()[catIdx];
            if (favs.isFavorite(entry.title)) {
                // Must cast const-away or call non-const fav manager
                const_cast<FavoritesManager&>(favs).removeFavorite(entry.title);
            } else {
                const_cast<FavoritesManager&>(favs).addFavorite(entry);
            }
            updateFilteredIndices(catalog, favs, recents);
        }
        
        // Touch events on bottom screen
        if (kDown & KEY_TOUCH) {
            u16 px = touch.px;
            u16 py = touch.py;
            
            // Check Letter Grid A-Z + #
            // Row 0-2 (y = 15 to 105)
            if (py >= 15 && py <= 105) {
                int col = (px - 10) / 34;
                int row = (py - 15) / 34;
                if (col >= 0 && col < 9 && row >= 0 && row < 3) {
                    int idx = row * 9 + col;
                    char letter = '#';
                    if (idx > 0 && idx < 27) {
                        letter = 'A' + (idx - 1);
                    }
                    
                    // Jump to matching index
                    int jumpIdx = catalog.getJumpIndex(letter);
                    if (jumpIdx != -1) {
                        // Find this primary index in the filtered indices
                        auto it = std::find(filtered_indices.begin(), filtered_indices.end(), jumpIdx);
                        if (it != filtered_indices.end()) {
                            selected_idx = static_cast<int>(std::distance(filtered_indices.begin(), it));
                            scroll_offset = selected_idx;
                            if (scroll_offset + 7 > static_cast<int>(filtered_indices.size())) {
                                scroll_offset = static_cast<int>(filtered_indices.size()) - 7;
                                if (scroll_offset < 0) scroll_offset = 0;
                            }
                        } else {
                            // If not in search filter, check standard search query clear
                            if (!search_query.empty()) {
                                search_query.clear();
                                updateFilteredIndices(catalog, favs, recents);
                            }
                            
                            // Find primary index again in now unfiltered list
                            auto it2 = std::find(filtered_indices.begin(), filtered_indices.end(), jumpIdx);
                            if (it2 != filtered_indices.end()) {
                                selected_idx = static_cast<int>(std::distance(filtered_indices.begin(), it2));
                                scroll_offset = selected_idx;
                                if (scroll_offset + 7 > static_cast<int>(filtered_indices.size())) {
                                    scroll_offset = static_cast<int>(filtered_indices.size()) - 7;
                                    if (scroll_offset < 0) scroll_offset = 0;
                                }
                            }
                        }
                    }
                }
            }
            // "Search" Button
            else if (px >= 10 && px <= 155 && py >= 125 && py <= 159) {
                std::string tempQuery = search_query;
                if (Search::showKeyboard(tempQuery)) {
                    search_query = tempQuery;
                    selected_idx = 0;
                    scroll_offset = 0;
                    updateFilteredIndices(catalog, favs, recents);
                }
            }
            // "Settings" Button
            else if (px >= 165 && px <= 310 && py >= 125 && py <= 159) {
                state = UIState::SETTINGS;
            }
            // Toggle Buttons: y = 168 to 202
            // "All Games"
            else if (px >= 10 && px <= 105 && py >= 168 && py <= 202) {
                list_type = MainListType::ALL_GAMES;
                selected_idx = 0;
                scroll_offset = 0;
                updateFilteredIndices(catalog, favs, recents);
            }
            // "Favorites"
            else if (px >= 112 && px <= 207 && py >= 168 && py <= 202) {
                list_type = MainListType::FAVORITES;
                selected_idx = 0;
                scroll_offset = 0;
                updateFilteredIndices(catalog, favs, recents);
            }
            // "Recents"
            else if (px >= 215 && px <= 310 && py >= 168 && py <= 202) {
                list_type = MainListType::RECENTS;
                selected_idx = 0;
                scroll_offset = 0;
                updateFilteredIndices(catalog, favs, recents);
            }
        }
        
    } else if (state == UIState::GAME_DETAILS) {
        int catIdx = filtered_indices[selected_idx];
        const auto& entry = catalog.getEntries()[catIdx];
        
        if (kDown & KEY_A) {
            // Trigger download
            if (const_cast<DownloadManager&>(downloader).startDownload(entry, config.download_dir, config.archive_cookie)) {
                state = UIState::DOWNLOADING;
            }
        }
        
        if (kDown & KEY_B) {
            state = UIState::MAIN_STOREFRONT;
        }
        
        if (kDown & KEY_X) {
            if (favs.isFavorite(entry.title)) {
                const_cast<FavoritesManager&>(favs).removeFavorite(entry.title);
            } else {
                const_cast<FavoritesManager&>(favs).addFavorite(entry);
            }
        }
        
        // Touch events
        if (kDown & KEY_TOUCH) {
            u16 px = touch.px;
            u16 py = touch.py;
            
            // "Download Game" button
            if (px >= 20 && px <= 300 && py >= 40 && py <= 76) {
                if (const_cast<DownloadManager&>(downloader).startDownload(entry, config.download_dir, config.archive_cookie)) {
                    state = UIState::DOWNLOADING;
                }
            }
            // "Add/Remove Favorite" button
            else if (px >= 20 && px <= 300 && py >= 86 && py <= 122) {
                if (favs.isFavorite(entry.title)) {
                    const_cast<FavoritesManager&>(favs).removeFavorite(entry.title);
                } else {
                    const_cast<FavoritesManager&>(favs).addFavorite(entry);
                }
            }
            // "Return to Catalog" button
            else if (px >= 20 && px <= 300 && py >= 132 && py <= 168) {
                state = UIState::MAIN_STOREFRONT;
            }
        }
        
    } else if (state == UIState::DOWNLOADING) {
        if (downloader.getStatus() == DownloadStatus::ERROR) {
            if (kDown & (KEY_A | KEY_B)) {
                const_cast<DownloadManager&>(downloader).reset();
                state = UIState::GAME_DETAILS;
            }
            if (kDown & KEY_TOUCH) {
                u16 px = touch.px;
                u16 py = touch.py;
                if (px >= 20 && px <= 300 && py >= 80 && py <= 130) {
                    const_cast<DownloadManager&>(downloader).reset();
                    state = UIState::GAME_DETAILS;
                }
            }
        } else {
            // Cancel downloads
            if (kDown & KEY_B) {
                const_cast<DownloadManager&>(downloader).cancelDownload();
                state = UIState::GAME_DETAILS;
            }
            
            // Touch events
            if (kDown & KEY_TOUCH) {
                u16 px = touch.px;
                u16 py = touch.py;
                
                // "CANCEL DOWNLOAD" button
                if (px >= 20 && px <= 300 && py >= 80 && py <= 130) {
                    const_cast<DownloadManager&>(downloader).cancelDownload();
                    state = UIState::GAME_DETAILS;
                }
            }
            
            // Verify success transitions
            if (downloader.getStatus() == DownloadStatus::COMPLETED) {
                // Add successfully downloaded to recents
                const_cast<RecentManager&>(recents).addRecent(downloader.getCurrentEntry());
                
                const_cast<DownloadManager&>(downloader).reset();
                state = UIState::GAME_DETAILS;
            }
        }
        
    } else if (state == UIState::SETTINGS) {
        if (kDown & KEY_DUP) {
            settings_selected_idx--;
            if (settings_selected_idx < 0) settings_selected_idx = 5;
        }
        if (kDown & KEY_DDOWN) {
            settings_selected_idx++;
            if (settings_selected_idx > 5) settings_selected_idx = 0;
        }
        
        if (kDown & KEY_B) {
            state = UIState::MAIN_STOREFRONT;
        }
        
        if (kDown & KEY_A) {
            if (settings_selected_idx == 0) {
                // Change ROM directory
                state = UIState::FIRST_LAUNCH_SETUP;
                setup_step = 1; // Go straight to file browser step
                current_dir = "sdmc:/";
                updateDirContents();
            }
            else if (settings_selected_idx == 1) {
                // Change Archive.org Cookies
                // Parse existing cookie if any
                auto parseCookieStr = [](const std::string& cookie, std::string& user, std::string& sig) {
                    user.clear();
                    sig.clear();
                    size_t userPos = cookie.find("logged-in-user=");
                    if (userPos != std::string::npos) {
                        size_t start = userPos + 15;
                        size_t semi = cookie.find(';', start);
                        if (semi != std::string::npos) {
                            user = cookie.substr(start, semi - start);
                        } else {
                            user = cookie.substr(start);
                        }
                    }
                    size_t sigPos = cookie.find("logged-in-sig=");
                    if (sigPos != std::string::npos) {
                        size_t start = sigPos + 14;
                        size_t semi = cookie.find(';', start);
                        if (semi != std::string::npos) {
                            sig = cookie.substr(start, semi - start);
                        } else {
                            sig = cookie.substr(start);
                        }
                    }
                };
                
                parseCookieStr(config.archive_cookie, setup_user, setup_sig);
                state = UIState::FIRST_LAUNCH_SETUP;
                setup_step = 3; // jump straight to input step
                WebServer::start();
            }
            else if (settings_selected_idx == 2) {
                // Refresh catalog now
                state = UIState::LOADING_CATALOG;
                render(catalog, favs, recents, downloader, config); // redraw loading frame
                
                if (const_cast<Catalog&>(catalog).load(true, config)) {
                    state = UIState::MAIN_STOREFRONT;
                    selected_idx = 0;
                    scroll_offset = 0;
                    updateFilteredIndices(catalog, favs, recents);
                } else {
                    // Fail gracefully
                    state = UIState::MAIN_STOREFRONT;
                }
            }
            else if (settings_selected_idx == 3) {
                // Clear Cached catalog
                const_cast<Catalog&>(catalog).clearCache();
                state = UIState::MAIN_STOREFRONT;
                filtered_indices.clear();
            }
            else if (settings_selected_idx == 4) {
                // Info dialog - do nothing, just visual
            }
            else if (settings_selected_idx == 5) {
                // Exit
                exit_app = true;
            }
        }
        
        // Touch events
        if (kDown & KEY_TOUCH) {
            u16 px = touch.px;
            u16 py = touch.py;
            
            // "Back to storefront" button
            if (px >= 20 && px <= 300 && py >= 195 && py <= 231) {
                state = UIState::MAIN_STOREFRONT;
            }
        }
    }
}
