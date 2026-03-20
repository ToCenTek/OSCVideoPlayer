/**
 * OSCPlayer - OSC protocol video player
 * Copyright (C) 2026 YHC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

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
