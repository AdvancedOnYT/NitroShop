#include <3ds.h>
#include "ui.hpp"
#include "configuration.hpp"
#include "networking.hpp"
#include "catalog.hpp"
#include "favorites.hpp"
#include "recent.hpp"
#include "download_manager.hpp"

int main(int argc, char** argv) {
    // Initialize RomFS (read-only filesystem inside 3dsx/cia) to access the Root CA certificates
    Result rc = romfsInit();
    if (R_FAILED(rc)) {
        // RomFS initialization failure, but we continue anyway
    }
    
    // Load local configuration
    AppConfig config = Configuration::load();
    
    // Initialize 3DS network SOC and curl states
    Networking::init();
    
    // Load local user lists
    FavoritesManager favs;
    favs.load();
    
    RecentManager recents;
    recents.load();
    
    Catalog catalog;
    DownloadManager downloader;
    
    // Initialize Citro2D/Citro3D UI framebuffers
    UI ui;
    if (!ui.init()) {
        Networking::exit();
        romfsExit();
        return 0;
    }
    
    // First Launch Setup or Main Storefront routing
    if (config.is_configured) {
        ui.setState(UIState::LOADING_CATALOG);
        ui.render(catalog, favs, recents, downloader, config);
        
        // Load local cache or download fresh pages if cache is >24 hours old
        bool loaded = catalog.load(false, config);
        if (loaded) {
            ui.setState(UIState::MAIN_STOREFRONT);
            ui.updateFilteredIndices(catalog, favs, recents);
        } else {
            // Fail gracefully: enter storefront screen anyway (will display empty list)
            ui.setState(UIState::MAIN_STOREFRONT);
        }
    } else {
        ui.setState(UIState::FIRST_LAUNCH_SETUP);
    }
    
    bool exit_app = false;
    
    // Primary 3DS OS Application Loop
    while (aptMainLoop()) {
        // Read button states, touchscreen position, and circle pad
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();
        
        touchPosition touch;
        hidTouchRead(&touch);
        
        circlePosition circle;
        hidCircleRead(&circle);
        
        // Process controller inputs
        ui.handleInput(kDown, kHeld, touch, circle, catalog, favs, recents, downloader, config, exit_app);
        
        // Post First Launch Setup transitions: Load catalog after path selection completes
        if (ui.getState() == UIState::LOADING_CATALOG) {
            ui.render(catalog, favs, recents, downloader, config);
            
            config = Configuration::load();
            bool loaded = catalog.load(false, config);
            if (loaded) {
                ui.setState(UIState::MAIN_STOREFRONT);
                ui.updateFilteredIndices(catalog, favs, recents);
            } else {
                ui.setState(UIState::MAIN_STOREFRONT);
            }
        }
        
                if (exit_app) {
            break;
        }
        
        // Render Frame graphics to CITRO3D top/bottom target framebuffers
        ui.render(catalog, favs, recents, downloader, config);
    }
    
    // Safely stop background download threads and join to prevent crashes on shutdown
    downloader.cancelDownload();
    
    // Shutdown system services
    ui.exit();
    Networking::exit();
    romfsExit();
    
    return 0;
}
