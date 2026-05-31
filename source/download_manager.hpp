#pragma once

#include <string>
#include <3ds.h>
#include "catalog.hpp"

enum class DownloadStatus {
    IDLE,
    DOWNLOADING,
    COMPLETED,
    CANCELLED,
    ERROR
};

class DownloadManager {
private:
    DownloadStatus status;
    uint64_t downloaded_bytes;
    uint64_t total_bytes;
    double download_speed; // in bytes per second
    double eta;            // in seconds
    bool cancel_requested;
    std::string error_msg;
    
    Thread thread;
    bool thread_running;
    
    CatalogEntry current_entry;
    std::string download_dir;
    std::string archive_cookie;
    
    static void downloadThreadEntry(void* arg);
    void runDownload();

public:
    DownloadManager();
    ~DownloadManager();
    
    // Starts the background download. Returns false if a download is already in progress.
    bool startDownload(const CatalogEntry& entry, const std::string& target_dir, const std::string& archive_cookie = "");
    
    // Requests cancellation. Blocks briefly until the thread exits and joins.
    void cancelDownload();
    
    // Clears the state back to IDLE
    void reset();
    void resetProgress(uint64_t downloaded, uint64_t total, double speed, double eta_val);
    
    DownloadStatus getStatus() const { return status; }
    uint64_t getDownloadedBytes() const { return downloaded_bytes; }
    uint64_t getTotalBytes() const { return total_bytes; }
    double getDownloadSpeed() const { return download_speed; }
    double getETA() const { return eta; }
    std::string getErrorMessage() const { return error_msg; }
    CatalogEntry getCurrentEntry() const { return current_entry; }
};
