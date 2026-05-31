#include "web_server.hpp"
#include <3ds.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cstdlib>

int WebServer::server_fd = -1;
int WebServer::port = 8080;
bool WebServer::is_running = false;

std::string WebServer::getLocalIP() {
    SOCU_IPInfo ipInfo;
    memset(&ipInfo, 0, sizeof(ipInfo));
    socklen_t optlen = sizeof(SOCU_IPInfo);
    int ret = SOCU_GetNetworkOpt(SOL_CONFIG, NETOPT_IP_INFO, &ipInfo, &optlen);
    if (ret == 0) {
        char ipStr[16];
        memset(ipStr, 0, sizeof(ipStr));
        if (inet_ntop(AF_INET, &ipInfo.ip, ipStr, sizeof(ipStr)) != nullptr) {
            return std::string(ipStr);
        }
    }
    return "Unknown (Connect to WiFi)";
}

bool WebServer::start(int p) {
    if (is_running) return true;
    port = p;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) return false;

    // Enable SO_REUSEADDR
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Set to non-blocking
    int flags = fcntl(server_fd, F_GETFL, 0);
    if (flags == -1 || fcntl(server_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        close(server_fd);
        server_fd = -1;
        return false;
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        close(server_fd);
        server_fd = -1;
        return false;
    }

    if (listen(server_fd, 3) < 0) {
        close(server_fd);
        server_fd = -1;
        return false;
    }

    is_running = true;
    return true;
}

void WebServer::stop() {
    if (!is_running) return;
    if (server_fd >= 0) {
        close(server_fd);
        server_fd = -1;
    }
    is_running = false;
}

static void sendAll(int fd, const std::string& data) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags != -1) {
        fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    }
    size_t total_sent = 0;
    while (total_sent < data.size()) {
        ssize_t sent = send(fd, data.c_str() + total_sent, data.size() - total_sent, 0);
        if (sent <= 0) {
            break;
        }
        total_sent += sent;
    }
}

bool WebServer::poll(std::string& outUser, std::string& outSig) {
    if (!is_running || server_fd < 0) return false;

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
    if (client_fd < 0) {
        return false; // No connection accepted
    }

    // Set client to non-blocking so recv doesn't block forever if select misbehaves
    int flags = fcntl(client_fd, F_GETFL, 0);
    if (flags != -1) {
        fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
    }

    std::string request;
    char buf[512];
    
    // Read request in a robust loop with select-based timeout (up to 1.0 second total)
    for (int attempt = 0; attempt < 20; ++attempt) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(client_fd, &read_fds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 50000; // 50ms timeout per select call

        int sel_ret = select(client_fd + 1, &read_fds, nullptr, nullptr, &tv);
        if (sel_ret > 0) {
            int n = recv(client_fd, buf, sizeof(buf) - 1, 0);
            if (n > 0) {
                buf[n] = '\0';
                request += buf;
                if (request.find("\r\n\r\n") != std::string::npos || request.find("\n\n") != std::string::npos) {
                    break; // End of HTTP headers
                }
            } else if (n == 0) {
                break; // Connection closed
            }
        } else if (sel_ret < 0) {
            break; // Select error
        } else {
            // Timeout (50ms). If we already have some bytes, check if headers are complete
            if (!request.empty()) {
                if (request.find("\r\n\r\n") != std::string::npos || request.find("\n\n") != std::string::npos) {
                    break;
                }
            }
        }
    }

    if (request.empty()) {
        close(client_fd);
        return false;
    }

    bool isSubmit = (request.find("GET /submit") != std::string::npos);
    bool parsedSuccess = false;

    if (isSubmit) {
        size_t submitPos = request.find("/submit?");
        std::string user = "";
        std::string sig = "";
        
        if (submitPos != std::string::npos) {
            size_t start = submitPos + 8;
            size_t end = request.find(" ", start);
            if (end != std::string::npos) {
                std::string query = request.substr(start, end - start);
                
                auto parseQueryParam = [](const std::string& q, const std::string& key) -> std::string {
                    size_t keyPos = q.find(key + "=");
                    if (keyPos == std::string::npos) return "";
                    size_t valStart = keyPos + key.size() + 1;
                    size_t ampPos = q.find("&", valStart);
                    if (ampPos != std::string::npos) {
                        return q.substr(valStart, ampPos - valStart);
                    }
                    return q.substr(valStart);
                };
                
                auto decodeParam = [](const std::string& str) -> std::string {
                    std::string result;
                    result.reserve(str.size());
                    for (size_t i = 0; i < str.size(); ++i) {
                        if (str[i] == '%' && i + 2 < str.size()) {
                            std::string hex = str.substr(i + 1, 2);
                            char chr = static_cast<char>(std::strtol(hex.c_str(), nullptr, 16));
                            result += chr;
                            i += 2;
                        } else if (str[i] == '+') {
                            result += ' ';
                        } else {
                            result += str[i];
                        }
                    }
                    return result;
                };
                
                user = decodeParam(parseQueryParam(query, "user"));
                sig = decodeParam(parseQueryParam(query, "sig"));
            }
        }

        if (!user.empty() && !sig.empty()) {
            outUser = user;
            outSig = sig;
            parsedSuccess = true;

            std::string html = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "Connection: close\r\n\r\n"
                "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>Success!</title><style>body{background-color:#0A0D14;color:#FFFFFF;font-family:-apple-system,BlinkMacSystemFont,sans-serif;margin:0;padding:20px;display:flex;justify-content:center;align-items:center;min-height:100vh;}.card{background-color:#151A26;border:1px solid #2ECC71;border-radius:12px;padding:30px;max-width:400px;width:100%;text-align:center;box-shadow:0 8px 32px rgba(46,204,113,0.15);}h1{color:#2ECC71;}p{color:#78909C;}</style></head><body><div class=\"card\"><h1>Success!</h1><p>Your Archive.org cookies have been successfully transmitted to your Nintendo 3DS.</p><p>Look at your 3DS screen. The application is now loading the catalog.</p></div></body></html>";
            
            sendAll(client_fd, html);
        } else {
            std::string html = 
                "HTTP/1.1 400 Bad Request\r\n"
                "Content-Type: text/html\r\n"
                "Connection: close\r\n\r\n"
                "<!DOCTYPE html><html><body><h1>Error</h1><p>Missing user or sig parameters.</p></body></html>";
            sendAll(client_fd, html);
        }
    } else {
        // Serve main input page
        std::string html = 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n\r\n"
            "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0\"><title>NitroShop Cookie Setup</title><style>body{background-color:#0A0D14;color:#FFFFFF;font-family:-apple-system,BlinkMacSystemFont,sans-serif;margin:0;padding:20px;display:flex;justify-content:center;align-items:center;min-height:100vh;box-sizing:border-box;}.card{background-color:#151A26;border:1px solid #00E5FF;border-radius:12px;padding:30px;max-width:500px;width:100%;box-shadow:0 8px 32px rgba(0,229,255,0.15);}h1{color:#00E5FF;margin-top:0;font-size:24px;text-shadow:0 0 10px rgba(0,229,255,0.3);}p{color:#78909C;font-size:14px;line-height:1.6;}.form-group{margin-bottom:20px;}label{display:block;margin-bottom:8px;font-weight:600;color:#00E5FF;font-size:14px;}input{background-color:#080A0F;border:1px solid #78909C;border-radius:6px;color:#FFFFFF;padding:12px;width:100%;box-sizing:border-box;font-size:14px;transition:border-color 0.3s,box-shadow 0.3s;}input:focus{outline:none;border-color:#00E5FF;box-shadow:0 0 8px rgba(0,229,255,0.4);}button{background-color:#2ECC71;color:#0A0D14;border:none;border-radius:6px;padding:14px;width:100%;font-size:16px;font-weight:bold;cursor:pointer;transition:transform 0.2s,background-color 0.3s;}button:hover{background-color:#27AE60;transform:translateY(-2px);}button:active{transform:translateY(0);}.note{background-color:#080A0F;border-left:4px solid #00E5FF;padding:12px;margin-bottom:20px;border-radius:0 6px 6px 0;}</style></head><body><div class=\"card\"><h1>NitroShop Cookie Portal</h1><p>Submit your Archive.org login session cookies to your Nintendo 3DS. This will automatically configure the application.</p><div class=\"note\"><p style=\"margin:0;color:#FFFFFF;font-weight:bold;\">Where to find these:</p><p style=\"margin:5px 0 0 0;\">Log in to Archive.org, open Browser Developer Tools (F12) -> Application/Storage -> Cookies -> Select archive.org. Copy values for <strong>logged-in-user</strong> and <strong>logged-in-sig</strong>.</p></div><form method=\"GET\" action=\"/submit\"><div class=\"form-group\"><label for=\"user\">logged-in-user (Email)</label><input type=\"text\" id=\"user\" name=\"user\" required placeholder=\"e.g. user%40example.com or user@example.com\"></div><div class=\"form-group\"><label for=\"sig\">logged-in-sig (Signature)</label><input type=\"text\" id=\"sig\" name=\"sig\" required placeholder=\"e.g. 5d9f0... (a long hexadecimal / text string)\"></div><button type=\"submit\">Submit to Nintendo 3DS</button></form></div></body></html>";
        
        sendAll(client_fd, html);
    }

    // Give socket 50ms to flush buffers before closing
    svcSleepThread(50000000ULL);
    close(client_fd);

    return parsedSuccess;
}
