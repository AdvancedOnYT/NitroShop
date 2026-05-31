#include "file_system.hpp"
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <algorithm>

namespace FileSystem {
    bool directoryExists(const std::string& path) {
        struct stat st;
        if (stat(path.c_str(), &st) == 0) {
            return S_ISDIR(st.st_mode);
        }
        return false;
    }

    bool fileExists(const std::string& path) {
        struct stat st;
        if (stat(path.c_str(), &st) == 0) {
            return S_ISREG(st.st_mode);
        }
        return false;
    }

    bool createDirectory(const std::string& path) {
        size_t pos = 0;
        do {
            pos = path.find('/', pos + 1);
            std::string subpath = path.substr(0, pos);
            if (!subpath.empty() && subpath != "sdmc:") {
                if (!directoryExists(subpath)) {
                    mkdir(subpath.c_str(), 0777);
                }
            }
        } while (pos != std::string::npos);
        return directoryExists(path);
    }

    uint64_t getFileSize(const std::string& path) {
        struct stat st;
        if (stat(path.c_str(), &st) == 0) {
            return st.st_size;
        }
        return 0;
    }

    bool deleteFile(const std::string& path) {
        return unlink(path.c_str()) == 0;
    }

    std::vector<std::string> getSubdirectories(const std::string& path) {
        std::vector<std::string> dirs;
        DIR* dir = opendir(path.c_str());
        if (!dir) return dirs;

        struct dirent* ent;
        while ((ent = readdir(dir)) != nullptr) {
            std::string name = ent->d_name;
            if (name == "." || name == "..") continue;

            std::string fullPath = path;
            if (fullPath.empty() || fullPath.back() != '/') {
                fullPath += "/";
            }
            fullPath += name;

            struct stat st;
            if (stat(fullPath.c_str(), &st) == 0) {
                if (S_ISDIR(st.st_mode)) {
                    dirs.push_back(name);
                }
            }
        }
        closedir(dir);
        std::sort(dirs.begin(), dirs.end());
        return dirs;
    }

    std::string getFileName(const std::string& path) {
        size_t pos = path.find_last_of('/');
        if (pos == std::string::npos) return path;
        return path.substr(pos + 1);
    }
}
