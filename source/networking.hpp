#pragma once

#include <string>

namespace Networking {
    bool init();
    void exit();
    bool isConnected();
    bool downloadString(const std::string& url, std::string& outResult, const std::string& cookie = "");
}
