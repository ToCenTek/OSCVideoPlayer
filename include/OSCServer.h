#ifndef OSC_SERVER_H
#define OSC_SERVER_H

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include "Platform.h"
#include "OSCMessage.h"

class Player;

class OSCServer {
public:
    OSCServer(const std::string& addr, int port, Player* player);
    ~OSCServer();

    bool start();
    void stop();
    void close();

private:
    void handleConnection();
    void handleMessage(const std::string& path, const OSCMessage& msg, const sockaddr_in& clientAddr);
    void sendResponse(const OSCMessage& msg, const sockaddr_in& clientAddr);
    void sendPosition(double pos);
    void sendPositionLoop();

    std::string addr_;
    int port_;
    Player* player_;
    int sockfd_ = -1;
    std::thread thread_;
    std::atomic<bool> running_{false};
    
    int replyPort_ = 0;
    std::string clientIP_;
    int seekingInterval_ = 500;
};

#endif
