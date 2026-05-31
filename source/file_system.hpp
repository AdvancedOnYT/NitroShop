#pragma once

#include <string>
#include <vector>

namespace FileSystem {
    bool directoryExists(const std::string& path);
    bool fileExists(const std::string& path);
    bool createDirectory(const std::string& path);
    uint64_t getFileSize(const std::string& path);
    bool deleteFile(const std::string& path);
    std::vector<std::string> getSubdirectories(const std::string& path);
    std::string getFileName(const std::string& path);
}
