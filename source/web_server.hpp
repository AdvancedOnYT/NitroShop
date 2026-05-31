#pragma once

#include <string>

class WebServer {
private:
    static int server_fd;
    static int port;
    static bool is_running;

public:
    static bool start(int port = 8080);
    static void stop();
    static std::string getLocalIP();
    static bool poll(std::string& outUser, std::string& outSig);
    static bool isRunning() { return is_running; }
};
