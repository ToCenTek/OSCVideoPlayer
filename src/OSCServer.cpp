#include "../include/OSCServer.h"
#include "../include/Player.h"
#include "../include/AudioDevice.h"
#include "../include/DisplayInfo.h"
#include "../include/Platform.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <sys/stat.h>
#include <csignal>
#include <chrono>

#ifndef _WIN32
#include <unistd.h>
#include <arpa/inet.h>
#else
#include <Windows.h>
#endif

OSCServer::OSCServer(const std::string& addr, int port, Player* player)
    : addr_(addr), port_(port), player_(player) {}

OSCServer::~OSCServer() {
    stop();
}

bool OSCServer::start() {
    if (!PlatformUtils::initNetwork()) {
        std::cerr << "Failed to initialize network" << std::endl;
        return false;
    }
    
    sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_ < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }
    
    int opt = 1;
    setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port_);
    
    if (bind(sockfd_, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        std::cerr << "Failed to bind to port " << port_ << std::endl;
#ifdef _WIN32
        CLOSE_SOCKET(sockfd_);
#else
        ::close(sockfd_);
#endif
        return false;
    }
    
    running_ = true;
    thread_ = std::thread(&OSCServer::handleConnection, this);
    std::thread(&OSCServer::sendPositionLoop, this).detach();
    
    std::cout << "OSC Server listening on " << addr_ << ":" << port_ << std::endl;
    return true;
}

void OSCServer::stop() {
    running_ = false;
    if (sockfd_ >= 0) {
#ifdef _WIN32
        CLOSE_SOCKET(sockfd_);
#else
        ::close(sockfd_);
#endif
        sockfd_ = -1;
    }
    if (thread_.joinable()) {
        thread_.join();
    }
    PlatformUtils::cleanupNetwork();
}

void OSCServer::close() {
    stop();
}

void OSCServer::handleConnection() {
    char buffer[4096];
    struct sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);
    
    while (running_) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sockfd_, &fds);
        struct timeval tv = {1, 0};
        
        int ret = select(sockfd_ + 1, &fds, NULL, NULL, &tv);
        if (ret <= 0) continue;
        
        ssize_t n = recvfrom(sockfd_, buffer, sizeof(buffer) - 1, 0, 
                            (struct sockaddr*)&clientAddr, &clientLen);
        if (n <= 0) continue;
        
        buffer[n] = 0;
        
        auto msg = parseOSCPacket(buffer, n);
        std::string path = msg.getAddress();
        
        std::cout << "[RECV] " << path;
        for (size_t i = 0; i < msg.argCount(); i++) {
            if (msg.getArgType(i) == OSCType::STRING) {
                std::cout << " \"" << msg.getStringArg(i) << "\"";
            } else if (msg.getArgType(i) == OSCType::INT32) {
                std::cout << " " << msg.getIntArg(i);
            } else if (msg.getArgType(i) == OSCType::FLOAT) {
                std::cout << " " << msg.getFloatArg(i);
            }
        }
        std::cout << std::endl;
        
        clientIP_ = inet_ntoa(clientAddr.sin_addr);
        
        handleMessage(path, msg, clientAddr);
    }
}

void OSCServer::handleMessage(const std::string& path, const OSCMessage& msg, const sockaddr_in& clientAddr) {
    std::string response;
    OSCMessage responseMsg;
    OSCMessage responseMsg2; // for extra response (e.g., /Pause after /stop)
    
    std::string cmd = path.substr(1);
    size_t slashPos = cmd.find('/');
    std::string mainCmd = slashPos != std::string::npos ? cmd.substr(0, slashPos) : cmd;
    std::string param = slashPos != std::string::npos ? cmd.substr(slashPos + 1) : "";
    
    for (auto& c : mainCmd) c = tolower(c);
    
    auto getArgString = [&]() -> std::string {
        if (msg.argCount() > 0) {
            if (msg.getArgType(0) == OSCType::STRING) {
                return msg.getStringArg(0);
            } else if (msg.getArgType(0) == OSCType::INT32) {
                return std::to_string(msg.getIntArg(0));
            } else if (msg.getArgType(0) == OSCType::FLOAT) {
                return std::to_string((int)msg.getFloatArg(0));
            }
        }
        return param;
    };
    
    auto getArgInt = [&](int defaultVal = 0) -> int {
        if (msg.argCount() > 0) {
            if (msg.getArgType(0) == OSCType::INT32) {
                return msg.getIntArg(0);
            } else if (msg.getArgType(0) == OSCType::FLOAT) {
                return (int)msg.getFloatArg(0);
            } else if (msg.getArgType(0) == OSCType::STRING) {
                return atoi(msg.getStringArg(0).c_str());
            }
        }
        if (!param.empty()) return atoi(param.c_str());
        return defaultVal;
    };
    
    if (mainCmd == "play") {
        std::string playParam = param;
        
        if (playParam.empty() && msg.argCount() > 0) {
            playParam = getArgString();
        }
        
        if (!playParam.empty()) {
            bool isAbsolutePath = false;
            
            if (playParam[0] == '/') {
                isAbsolutePath = true;
            } else if (playParam.find("Users/") == 0 || playParam.find("Volume/") == 0) {
                playParam = "/" + playParam;
                isAbsolutePath = true;
            }
            
            if (isAbsolutePath) {
                struct stat st;
                if (stat(playParam.c_str(), &st) != 0) {
                    responseMsg.setAddress("/Error");
                    responseMsg.addString("Video not found: " + param);
                    sendResponse(responseMsg, clientAddr);
    
    if (responseMsg2.getArgs().size() > 0) {
        sendResponse(responseMsg2, clientAddr);
    }
                    return;
                }
                size_t pos = playParam.find_last_of('/');
                playParam = (pos != std::string::npos) ? playParam.substr(pos + 1) : playParam;
            } else if (playParam.find('/') != std::string::npos) {
                std::vector<std::string> paths = getVideoSearchPaths();
                
                bool found = false;
                for (const auto& baseDir : paths) {
                    std::string fullPath = baseDir + "/" + playParam;
                    struct stat st;
                    if (stat(fullPath.c_str(), &st) == 0) {
                        size_t pos = playParam.find_last_of('/');
                        playParam = (pos != std::string::npos) ? playParam.substr(pos + 1) : playParam;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    responseMsg.setAddress("/Error");
                    responseMsg.addString("Video not found: " + param);
                    sendResponse(responseMsg, clientAddr);
    
    if (responseMsg2.getArgs().size() > 0) {
        sendResponse(responseMsg2, clientAddr);
    }
                    return;
                }
            } else {
                std::vector<std::string> all = getAllVideos();
                
                bool found = false;
                for (const auto& v : all) {
                    size_t pos = v.find_last_of('/');
                    std::string fname = (pos != std::string::npos) ? v.substr(pos + 1) : v;
                    
                    if (fname == playParam) {
                        playParam = fname;
                        found = true;
                        break;
                    }
                    size_t extPos = fname.rfind('.');
                    if (extPos != std::string::npos) {
                        std::string baseName = fname.substr(0, extPos);
                        if (baseName == playParam) {
                            playParam = fname;
                            found = true;
                            break;
                        }
                    }
                }
                if (!found && !all.empty()) {
                    responseMsg.setAddress("/Error");
                    responseMsg.addString("Video not found: " + param);
                    sendResponse(responseMsg, clientAddr);
    
    if (responseMsg2.getArgs().size() > 0) {
        sendResponse(responseMsg2, clientAddr);
    }
                    return;
                }
            }
        }
        player_->Play(playParam);
        
        bool isLoop = player_->IsLoopEnabled();
        OSCMessage loopMsg;
        loopMsg.setAddress("/Loop");
        loopMsg.addBool(isLoop);
        sendResponse(loopMsg, clientAddr);
        
        responseMsg.setAddress("/Playing");
        responseMsg.addString(playParam.empty() ? player_->GetCurrentFile() : playParam);
    }
    else if (mainCmd == "list") {
        std::string argStr = getArgString();
        
        if (argStr == "audio") {
            auto devs = getAudioDevicesWithIndex();
            responseMsg.setAddress("/AudioList");
            std::string listStr;
            for (size_t i = 0; i < devs.size(); i++) {
                if (i > 0) listStr += ", ";
                listStr += devs[i];
            }
            responseMsg.addString(listStr);
        } else if (argStr == "display") {
            auto displays = getDisplays();
            responseMsg.setAddress("/DisplayList");
            std::string listStr;
            for (size_t i = 0; i < displays.size(); i++) {
                if (i > 0) listStr += ", ";
                listStr += std::to_string(i) + ":" + displays[i];
            }
            responseMsg.addString(listStr);
        } else if (argStr == "videos") {
            std::vector<std::string> videos = getAllVideos();
            responseMsg.setAddress("/Videos");
            std::string allVideos;
            for (size_t i = 0; i < videos.size(); i++) {
                if (i > 0) allVideos += "\n";
                allVideos += "\"" + videos[i] + "\"";
            }
            responseMsg.addString(allVideos);
        } else {
            responseMsg.setAddress("/Error");
            responseMsg.addString("Unknown list type");
        }
    }
    else if (mainCmd == "stop") {
        bool hasArg = msg.argCount() > 0;
        
        if ((param.empty() || param == "hello") && !hasArg) {
            player_->SetStopMode(-1);
            player_->Stop();
            responseMsg.setAddress("/Stopped");
            responseMsg.addString("Seek to " + player_->getHelloVideoName());
            responseMsg.addBool(player_->IsPaused());
            responseMsg2.setAddress("/Pause");
            responseMsg2.addBool(player_->IsPaused());
        } else if ((param == "0" || param == "exit") && !hasArg) {
            player_->SetStopMode(2);
            player_->StopExit();
            responseMsg.setAddress("/Stopped");
            responseMsg.addString("Exit window");
            responseMsg.addBool(true);
            responseMsg2.setAddress("/Pause");
            responseMsg2.addBool(true);
        } else {
            double value = 0;
            bool isTime = false;
            bool isExit = false;
            
            if (hasArg) {
                if (msg.getArgType(0) == OSCType::INT32) {
                    value = msg.getIntArg(0);
                } else {
                    value = msg.getFloatArg(0);
                    isTime = true;
                }
                if (value == 0) isExit = true;
            } else if (param == "0" || param == "exit") {
                isExit = true;
            } else if (param.find('.') != std::string::npos) {
                value = atof(param.c_str());
                isTime = true;
            } else {
                value = atoi(param.c_str());
            }
            
            if (isExit) {
                player_->SetStopMode(2);
                player_->StopExit();
                responseMsg.setAddress("/Stopped");
                responseMsg.addString("Exit window");
                responseMsg.addBool(true);
                responseMsg2.setAddress("/Pause");
                responseMsg2.addBool(true);
            } else if (isTime) {
                player_->SetStopMode((int)(value * 1000));
                std::string result = player_->StopAtSeconds(value);
                responseMsg.setAddress("/Stopped");
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(3) << value << " s";
                responseMsg.addString(oss.str());
                responseMsg.addBool(player_->IsPaused());
                responseMsg2.setAddress("/Pause");
                responseMsg2.addBool(player_->IsPaused());
            } else {
                player_->SetStopMode((int)value);
                std::string result = player_->StopAtFrame((int)value);
                responseMsg.setAddress("/Stopped");
                responseMsg.addString(result + " frame");
                responseMsg.addBool(player_->IsPaused());
                responseMsg2.setAddress("/Pause");
                responseMsg2.addBool(player_->IsPaused());
            }
        }
    }
    else if (mainCmd == "pause") {
        bool isPaused = player_->IsPaused();
        
        if (msg.argCount() > 0) {
            int action = 0;
            if (msg.getArgType(0) == OSCType::INT32) {
                action = msg.getIntArg(0);
            } else {
                action = (int)msg.getFloatArg(0);
            }
            if (action == 0) {
                if (isPaused) player_->Pause();
            } else if (action == 1) {
                if (!isPaused) player_->Pause();
            }
        } else if (!param.empty()) {
            int action = atoi(param.c_str());
            if (action == 0) {
                if (isPaused) player_->Pause();
            } else if (action == 1) {
                if (!isPaused) player_->Pause();
            }
        } else {
            player_->Pause();
        }
        
        responseMsg.setAddress("/Pause");
        responseMsg.addBool(player_->IsPaused());
    }
    else if (mainCmd == "vol" || mainCmd == "volume") {
        float vol = 0.5f;
        if (msg.argCount() > 0) {
            vol = msg.getFloatArg(0);
        } else if (!param.empty()) {
            vol = atof(param.c_str());
        }
        vol = std::max(0.0f, std::min(1.0f, vol));
        
        player_->SetVolume((int)(vol * 100));
        
        responseMsg.setAddress("/Volume");
        responseMsg.addString(std::to_string((int)(vol * 100)) + " %");
    }
    else if (mainCmd == "loop") {
        bool enable = true;
        int intVal = getArgInt(1);
        enable = (intVal != 0);
        player_->SetLoop(enable);
        responseMsg.setAddress("/Loop");
        responseMsg.addBool(enable);
    }
    else if (mainCmd == "fullscreen") {
        std::string argStr = getArgString();
        size_t slashPos = argStr.find('/');
        if (slashPos != std::string::npos) {
            argStr = argStr.substr(0, slashPos);
        }
        int mode = atoi(argStr.c_str());
        player_->SetFullscreen(mode);
        responseMsg.setAddress("/Fullscreen");
        responseMsg.addInt(mode);
    }
    else if (mainCmd == "seek") {
        float value = 0.0f;
        bool isInteger = false;
        bool isIntArg = false;
        
        if (msg.argCount() > 0) {
            if (msg.getArgType(0) == OSCType::INT32) {
                value = (float)msg.getIntArg(0);
                isInteger = true;
                isIntArg = true;
            } else if (msg.getArgType(0) == OSCType::FLOAT) {
                value = msg.getFloatArg(0);
            }
        } else if (!param.empty()) {
            value = atof(param.c_str());
            isInteger = (param.find('.') == std::string::npos);
            if (isInteger) {
                isIntArg = true;
            }
        }
        
        std::ostringstream oss;
        
        if (isInteger && isIntArg) {
            player_->SeekToFrame((int)value);
            if (value >= 0) {
                oss << (int)value << " frame forward";
            } else {
                oss << (int)(-value) << " frame backward";
            }
        } else if (value > 0 && value < 1.0f) {
            player_->SeekToPercent(value * 100);
            oss << std::fixed << std::setprecision(1) << (value * 100) << " %";
        } else if (value >= 1.0f) {
            player_->SeekToTime(value);
            oss << std::fixed << std::setprecision(1) << value << " s forward";
        } else {
            player_->SeekToTime(value);
            oss << std::fixed << std::setprecision(1) << (-value) << " s backward";
        }
        
        if (player_->IsPaused()) {
            player_->Pause();
        }
        
        responseMsg.setAddress("/SeekTo");
        responseMsg.addString(oss.str());
        
        responseMsg2.setAddress("/Pause");
        responseMsg2.addBool(false);
    }
    else if (mainCmd == "audio") {
        std::string audioParam;
        if (msg.argCount() > 0) {
            if (msg.getArgType(0) == OSCType::INT32) {
                audioParam = std::to_string(msg.getIntArg(0));
            } else if (msg.getArgType(0) == OSCType::FLOAT) {
                audioParam = std::to_string((int)msg.getFloatArg(0));
            } else {
                audioParam = msg.getStringArg(0);
            }
        } else {
            audioParam = param;
        }
        
        if (audioParam.empty() || audioParam == "list") {
            auto devs = getAudioDevicesWithIndex();
            responseMsg.setAddress("/AudioList");
            std::string listStr;
            for (size_t i = 0; i < devs.size(); i++) {
                if (i > 0) listStr += ", ";
                listStr += devs[i];
            }
            responseMsg.addString(listStr);
        } else {
            int idx = atoi(audioParam.c_str());
            std::string deviceID;
            std::string displayName;
            
            if (idx >= 0) {
                deviceID = getAudioDeviceIDByIndex(idx);
                if (!deviceID.empty()) {
                    displayName = getAudioDevices()[idx].displayName;
                }
            } else {
                deviceID = getAudioDeviceIDByName(param);
                displayName = param;
            }
            
            if (!deviceID.empty()) {
                player_->SetAudioDevice(deviceID);
                responseMsg.setAddress("/Audio");
                responseMsg.addString(displayName);
            } else {
                deviceID = getAudioDeviceIDByName(audioParam);
                if (!deviceID.empty()) {
                    player_->SetAudioDevice(deviceID);
                    responseMsg.setAddress("/Audio");
                    responseMsg.addString(audioParam);
                } else {
                    responseMsg.setAddress("/Error");
                    responseMsg.addString("Audio device not found");
                }
            }
        }
    }
    else if (mainCmd == "display") {
        std::string displayParam;
        if (msg.argCount() > 0) {
            if (msg.getArgType(0) == OSCType::INT32) {
                displayParam = std::to_string(msg.getIntArg(0));
            } else if (msg.getArgType(0) == OSCType::FLOAT) {
                displayParam = std::to_string((int)msg.getFloatArg(0));
            } else {
                displayParam = msg.getStringArg(0);
            }
        } else {
            displayParam = param;
        }
        
        if (displayParam.empty() || displayParam == "list") {
            auto displays = getDisplays();
            responseMsg.setAddress("/DisplayList");
            std::string listStr;
            for (size_t i = 0; i < displays.size(); i++) {
                if (i > 0) listStr += ", ";
                listStr += std::to_string(i) + ":" + displays[i];
            }
            responseMsg.addString(listStr);
        } else {
            int idx = atoi(displayParam.c_str());
            auto displays = getDisplays();
            if (idx >= 0 && idx < (int)displays.size()) {
                // Pass the index as string instead of device name
                player_->SetDisplay(std::to_string(idx));
                responseMsg.setAddress("/Display");
                responseMsg.addString(displays[idx]);
            } else {
                bool found = false;
                for (size_t i = 0; i < displays.size(); i++) {
                    if (displays[i].find(displayParam) != std::string::npos) {
                        player_->SetDisplay(displays[i]);
                        responseMsg.setAddress("/Display");
                        responseMsg.addString(displays[i]);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    responseMsg.setAddress("/Error");
                    responseMsg.addString("Display not found");
                }
            }
        }
    }
    else if (mainCmd == "fps") {
        int fpsMode = -1;
        
        if (!param.empty()) {
            fpsMode = atoi(param.c_str());
        } else if (msg.argCount() > 0) {
            if (msg.getArgType(0) == OSCType::INT32) {
                fpsMode = msg.getIntArg(0);
            } else if (msg.getArgType(0) == OSCType::FLOAT) {
                fpsMode = (int)msg.getFloatArg(0);
            }
        }
        
        if (fpsMode == 2) {
            player_->SetFPSDisplay(2);
            std::string timing = player_->GetFrameTiming();
            double last = 0, avg = 0, peak = 0;
            char buf[256];
            if (sscanf(timing.c_str(), "%lf/%lf/%lf", &last, &avg, &peak) == 3) {
                double last_fps = last > 0 ? 1000000.0 / last : 0;
                double avg_fps = avg > 0 ? 1000000.0 / avg : 0;
                double peak_fps = peak > 0 ? 1000000.0 / peak : 0;
                snprintf(buf, sizeof(buf), "Last:%.6f Avg:%.6f Peak:%.6f", last_fps, avg_fps, peak_fps);
            } else {
                snprintf(buf, sizeof(buf), "No frame data");
            }
            responseMsg.setAddress("/FPS");
            responseMsg.addString(buf);
        } else if (fpsMode == 0) {
            player_->SetFPSDisplay(0);
            responseMsg.setAddress("/FPS");
            responseMsg.addString("FPS display OFF");
        } else if (fpsMode > 0) {
            player_->SetFPSDisplay(fpsMode);
            std::string timing = player_->GetFrameTiming();
            double last = 0, avg = 0, peak = 0;
            char buf[128];
            if (sscanf(timing.c_str(), "%lf/%lf/%lf", &last, &avg, &peak) == 3 && last > 0) {
                snprintf(buf, sizeof(buf), "FPS display ON - Last:%dus Avg:%dus Peak:%dus", (int)last, (int)avg, (int)peak);
            } else {
                snprintf(buf, sizeof(buf), "FPS display ON");
            }
            responseMsg.setAddress("/FPS");
            responseMsg.addString(buf);
        } else {
            std::string timing = player_->GetFrameTiming();
            double last = 0, avg = 0, peak = 0;
            char buf[128];
            if (sscanf(timing.c_str(), "%lf/%lf/%lf", &last, &avg, &peak) == 3) {
                if (last > 0) {
                    snprintf(buf, sizeof(buf), "Last:%dus Avg:%dus Peak:%dus", (int)last, (int)avg, (int)peak);
                } else {
                    snprintf(buf, sizeof(buf), "FPS: %d", (int)(avg > 0 ? 1000000/avg : 0));
                }
            } else {
                snprintf(buf, sizeof(buf), "No frame data");
            }
            responseMsg.setAddress("/FPS");
            responseMsg.addString(buf);
        }
    }
    else if (mainCmd == "seeking") {
        int mode = -1;
        
        if (!param.empty()) {
            mode = atoi(param.c_str());
        } else if (msg.argCount() > 0) {
            if (msg.getArgType(0) == OSCType::INT32) {
                mode = msg.getIntArg(0);
            } else if (msg.getArgType(0) == OSCType::FLOAT) {
                mode = (int)msg.getFloatArg(0);
            }
        }
        
        if (mode > 0) {
            seekingInterval_ = mode;
            responseMsg.setAddress("/FPS");
            responseMsg.addString("Seeking interval: " + std::to_string(mode) + "ms");
        } else if (mode == 0) {
            seekingInterval_ = 0;
            responseMsg.setAddress("/FPS");
            responseMsg.addString("Seeking OFF");
        } else {
            responseMsg.setAddress("/FPS");
            responseMsg.addString("Seeking interval: " + std::to_string(seekingInterval_) + "ms");
        }
    }
    else if (mainCmd == "frametiming") {
        std::string timing = player_->GetFrameTiming();
        responseMsg.setAddress("/FrameTiming");
        responseMsg.addString(timing);
    }
    else if (mainCmd == "tct") {
        std::string text = param;
        int fontSize = 48;
        int position = 0;
        
        if (msg.argCount() >= 1) {
            if (msg.getArgType(0) == OSCType::STRING) {
                text = msg.getStringArg(0);
                if (msg.argCount() >= 2) {
                    fontSize = (int)msg.getFloatArg(1);
                }
                if (msg.argCount() >= 3) {
                    position = (int)msg.getFloatArg(2);
                }
            } else if (msg.argCount() == 1) {
                int v = msg.getIntArg(0);
                if (v == 0) {
                    player_->ShowText("", 0, false, 0);
                    responseMsg.setAddress("/TCT");
                    responseMsg.addString("F");
                } else {
                    player_->ShowText("", 0, false, 0);
                    responseMsg.setAddress("/TCT");
                    responseMsg.addString("F");
                }
            }
        }
        
        if (text.empty() || text == "0" || text == "off") {
            player_->ShowText("", 0, false, 0);
            responseMsg.setAddress("/TCT");
            responseMsg.addString("F");
        } else {
            if (fontSize <= 0) fontSize = 48;
            player_->ShowText(text, fontSize, true, position);
            responseMsg.setAddress("/TCT");
            responseMsg.addString("T");
        }
    } else if (false) { // PLACEHOLDER DELETE ME
        std::string text;
        int fontSize = 48;
        int position = 0;
        
        bool allNumeric = true;
        for (size_t i = 0; i < msg.argCount(); i++) {
            if (msg.getArgType(i) == OSCType::STRING) {
                allNumeric = false;
                break;
            }
        }
        
        if (allNumeric && msg.argCount() >= 1) {
            if (msg.argCount() == 1) {
                int v = msg.getIntArg(0);
                if (v == 0) {
                    player_->ShowText("", 0, false, 0);
                    responseMsg.setAddress("/TCT");
                    responseMsg.addString("F");
                } else {
                    player_->ShowText("", 0, false, 0);
                    responseMsg.setAddress("/TCT");
                    responseMsg.addString("F");
                }
            } else if (msg.argCount() >= 2) {
                fontSize = (int)msg.getFloatArg(0);
                if (msg.argCount() >= 3) {
                    position = (int)msg.getFloatArg(1);
                }
                player_->ShowText(param.empty() ? "text" : param, fontSize, true, position);
                responseMsg.setAddress("/TCT");
                responseMsg.addString("T");
            }
        } else if (msg.argCount() >= 1 && msg.getArgType(0) == OSCType::STRING) {
            text = msg.getStringArg(0);
            
            if (msg.argCount() >= 2) {
                fontSize = (int)msg.getFloatArg(1);
            }
            
            if (msg.argCount() >= 3) {
                position = (int)msg.getFloatArg(2);
            }
            
            if (!text.empty() && text != "0" && text != "off") {
                player_->ShowText(text, fontSize, true, position);
                responseMsg.setAddress("/TCT");
                responseMsg.addString("T");
            } else {
                player_->ShowText("", 0, false, 0);
                responseMsg.setAddress("/TCT");
                responseMsg.addString("F");
            }
        } else if (!param.empty()) {
            std::vector<std::string> parts;
            std::string temp = param;
            size_t pos;
            while ((pos = temp.find('/')) != std::string::npos) {
                parts.push_back(temp.substr(0, pos));
                temp = temp.substr(pos + 1);
            }
            if (!temp.empty()) parts.push_back(temp);
            
            if (parts.size() == 3) {
                text = parts[0];
                fontSize = atoi(parts[1].c_str());
                position = atoi(parts[2].c_str());
                
            } else if (parts.size() == 2) {
                text = parts[0];
                fontSize = atoi(parts[1].c_str());
                
            } else if (parts.size() == 1 && parts[0] != "0" && parts[0] != "off") {
                text = parts[0];
                
            }
        }
        
        if (text.empty() || text == "0" || text == "off") {
            player_->ShowText("", 0, false, 0);
            responseMsg.setAddress("/TCT");
            responseMsg.addString("F");
        } else {
            if (fontSize <= 0) fontSize = 48;
            player_->ShowText(text, fontSize, true, position);
            responseMsg.setAddress("/TCT");
            responseMsg.addString("T");
        }
    }
    else if (mainCmd == "seeking") {
        bool enable = true;
        if (msg.argCount() > 0) {
            if (msg.getArgType(0) == OSCType::INT32) {
                enable = msg.getIntArg(0) != 0;
            } else {
                enable = msg.getFloatArg(0) != 0.0f;
            }
        } else if (!param.empty()) {
            enable = (param != "0");
        }
        player_->seekingEnabled_ = enable;
    }
    else if (mainCmd == "reboot") {
        responseMsg.setAddress("/Reboot");
        responseMsg.addString("Rebooting...");
        
        std::cout << "[SEND] " << responseMsg.getAddress() << " " << responseMsg.getArgs()[0].stringVal << std::endl;
        sendResponse(responseMsg, clientAddr);
        
        if (responseMsg2.getArgs().size() > 0) {
            sendResponse(responseMsg2, clientAddr);
        }
        
        std::cout << "Initiating system reboot..." << std::endl;
        
        int pid = player_->GetChildPid();
        if (pid > 0) {
#ifdef _WIN32
            HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
            if (h) {
                TerminateProcess(h, 1);
                CloseHandle(h);
            }
            Sleep(200);
            system("taskkill /F /IM mpv.exe 2>NUL");
#else
            kill(-pid, SIGTERM);
            usleep(100000);
            kill(-pid, SIGKILL);
#endif
        }
        
        running_ = false;
        if (sockfd_ >= 0) {
#ifdef _WIN32
            CLOSE_SOCKET(sockfd_);
#else
            ::close(sockfd_);
#endif
            sockfd_ = -1;
        }
        
#ifdef __linux__
        system("systemctl reboot &");
#elif _WIN32
        system("shutdown /r /t 0");
#elif __APPLE__
        system("osascript -e 'tell app \"System Events\" to restart' &");
#endif
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        ::_exit(0);
    }
    else if (mainCmd == "help") {
        std::string help = R"(
/play[xxx]       - Play video (xxx = filename or path)
/stop[xxx]       - Stop, goto frame/time or exit (xxx = frame/time/0/exit)
/pause           - Toggle pause
/volume[x]       - Set volume (0.0-1.0)
/loop[x]         - Enable/disable loop (0/1)
/fullscreen[x]   - Set display mode (0=window, 1=v-stretch, 2=h-stretch, 3=fullscreen)
/seek[x]         - Seek to position (percent/time/frame)
/audio[x]        - Set audio device (x = device index or name)
/display[x]      - Set display (x = display index or name)
/fps[x]          - Set FPS display mode (0=off, 1=display)
/seeking[x]      - Set seeking interval in ms (0=off)
/tct[text/size/pos] - Show text on screen (pos: 0=center, 1=top-left, 2=top-right, 3=bottom-left, 4=bottom-right)
/port[name/port] - Set reply port (e.g., chataigne/12000)
/list/audio      - List audio devices
/list/display   - List displays
/list/videos    - List all videos
/reboot          - Reboot system
/shutdown        - Shutdown system
)";
        responseMsg.setAddress("/Help");
        responseMsg.addString(help);
    }
    else if (mainCmd == "shutdown") {
        responseMsg.setAddress("/Shutdown");
        responseMsg.addString("Power off");
        
        std::cout << "[SEND] " << responseMsg.getAddress() << " " << responseMsg.getArgs()[0].stringVal << std::endl;
        sendResponse(responseMsg, clientAddr);
        
        if (responseMsg2.getArgs().size() > 0) {
            sendResponse(responseMsg2, clientAddr);
        }
        
        std::cout << "Initiating system shutdown..." << std::endl;
        
        int pid = player_->GetChildPid();
        if (pid > 0) {
#ifdef _WIN32
            HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
            if (h) {
                TerminateProcess(h, 1);
                CloseHandle(h);
            }
            Sleep(200);
            system("taskkill /F /IM mpv.exe 2>NUL");
#else
            kill(-pid, SIGTERM);
            usleep(100000);
            kill(-pid, SIGKILL);
#endif
        }
        
        running_ = false;
        if (sockfd_ >= 0) {
#ifdef _WIN32
            CLOSE_SOCKET(sockfd_);
#else
            ::close(sockfd_);
#endif
            sockfd_ = -1;
        }
        
#ifdef __linux__
        system("systemctl poweroff &");
#elif _WIN32
        system("shutdown /s /t 0");
#elif __APPLE__
        system("osascript -e 'tell app \"System Events\" to shutdown' &");
#endif
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        ::_exit(0);
    }
    else if (mainCmd == "port") {
        std::string name = "";
        int port = 0;
        
        if (msg.argCount() >= 1) {
            if (msg.getArgType(0) == OSCType::STRING) {
                name = msg.getStringArg(0);
                if (msg.argCount() >= 2) {
                    port = (int)msg.getFloatArg(1);
                }
            } else {
                port = msg.getIntArg(0);
            }
        }
        
        if (port == 0 && !param.empty()) {
            size_t lastSlash = param.rfind('/');
            if (lastSlash != std::string::npos && lastSlash > 0) {
                name = param.substr(0, lastSlash);
                port = atoi(param.substr(lastSlash + 1).c_str());
            } else {
                port = atoi(param.c_str());
            }
        }
        
        if (port >= 1024 && port <= 65535) {
            replyPort_ = port;
            responseMsg.setAddress("/OSCPlayer");
            if (name.empty()) {
                responseMsg.addString("Hello on port " + std::to_string(port));
            } else {
                responseMsg.addString("Hello, " + name + ":" + std::to_string(port));
            }
        } else {
            responseMsg.setAddress("/Error");
            responseMsg.addString("Invalid Port number!");
        }
    }
    else {
        responseMsg.setAddress("/Unknown");
        responseMsg.addString(path);
    }
    
    if (responseMsg.getAddress().empty()) return;
    
    std::cout << "[SEND] " << responseMsg.getAddress();
    for (size_t i = 0; i < responseMsg.argCount(); i++) {
        auto& arg = responseMsg.getArgs()[i];
        if (arg.type == OSCType::STRING) {
            std::cout << " " << arg.stringVal;
        } else if (arg.type == OSCType::INT32) {
            std::cout << " " << arg.intVal;
        } else if (arg.type == OSCType::FLOAT) {
            std::cout << " " << arg.floatVal;
        }
    }
    std::cout << std::endl;
    
    sendResponse(responseMsg, clientAddr);
    
    if (responseMsg2.getArgs().size() > 0) {
        sendResponse(responseMsg2, clientAddr);
    }
}

static std::string padTo4(const std::string& s) {
    size_t len = s.size() + 1;
    size_t pad = (4 - (len % 4)) % 4;
    std::string result = s;
    result.push_back('\0');
    result.append(pad, '\0');
    return result;
}

static void appendOSCValue(std::vector<char>& packet, const OSCArg& arg) {
    char buf[8];
    if (arg.type == OSCType::BOOL) {
        // OSC布尔类型在数据部分没有字节，只是类型标签中的T/F
        return;
    } else if (arg.type == OSCType::INT32) {
        int32_t v = arg.intVal;
        uint32_t uv = static_cast<uint32_t>(v);
        buf[0] = static_cast<char>((uv >> 24) & 0xFF);
        buf[1] = static_cast<char>((uv >> 16) & 0xFF);
        buf[2] = static_cast<char>((uv >> 8) & 0xFF);
        buf[3] = static_cast<char>((uv) & 0xFF);
        packet.insert(packet.end(), buf, buf + 4);
    } else if (arg.type == OSCType::FLOAT) {
        union { float f; uint32_t u; } conv;
        conv.f = arg.floatVal;
        uint32_t v = conv.u;
        buf[0] = static_cast<char>((v >> 24) & 0xFF);
        buf[1] = static_cast<char>((v >> 16) & 0xFF);
        buf[2] = static_cast<char>((v >> 8) & 0xFF);
        buf[3] = static_cast<char>((v) & 0xFF);
        packet.insert(packet.end(), buf, buf + 4);
    } else if (arg.type == OSCType::STRING) {
        std::string s = padTo4(arg.stringVal);
        packet.insert(packet.end(), s.begin(), s.end());
    }
}

void OSCServer::sendResponse(const OSCMessage& msg, const sockaddr_in& clientAddr) {
    std::string addr = padTo4(msg.getAddress());
    
    std::string typeTag = ",";
    for (size_t i = 0; i < msg.argCount(); i++) {
        auto& arg = msg.getArgs()[i];
        if (arg.type == OSCType::BOOL) typeTag += arg.boolVal ? "T" : "F";
        else if (arg.type == OSCType::INT32) typeTag += "i";
        else if (arg.type == OSCType::FLOAT) typeTag += "f";
        else if (arg.type == OSCType::STRING) typeTag += "s";
    }
    typeTag = padTo4(typeTag);
    
    std::vector<char> packet;
    packet.insert(packet.end(), addr.begin(), addr.end());
    packet.insert(packet.end(), typeTag.begin(), typeTag.end());
    
    for (size_t i = 0; i < msg.argCount(); i++) {
        appendOSCValue(packet, msg.getArgs()[i]);
    }
    
    struct sockaddr_in target;
    target = clientAddr;
    if (replyPort_ > 0) {
        target.sin_port = htons(replyPort_);
    }
    
    sendto(sockfd_, packet.data(), packet.size(), 0, (struct sockaddr*)&target, sizeof(target));
}

void OSCServer::sendPositionLoop() {
    while (running_) {
        if (seekingInterval_ > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(seekingInterval_));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        if (seekingInterval_ == 0) continue;
        if (!player_->seekingEnabled_) continue;
        if (!player_->IsPlaying()) continue;
        if (player_->IsPaused()) continue;
        if (replyPort_ == 0 || clientIP_.empty()) continue;
        
        double time = player_->GetTime();
        if (time > 0) {
            sendPosition(time);
        }
    }
}

void OSCServer::sendPosition(double pos) {
    if (sockfd_ < 0 || replyPort_ == 0 || clientIP_.empty()) return;
    
    std::string addr = padTo4("/Seeking");
    std::string typeTag = padTo4(",f");
    
    std::vector<char> packet;
    packet.insert(packet.end(), addr.begin(), addr.end());
    packet.insert(packet.end(), typeTag.begin(), typeTag.end());
    
    OSCArg arg;
    arg.type = OSCType::FLOAT;
    arg.floatVal = (float)pos;
    appendOSCValue(packet, arg);
    
    struct sockaddr_in target;
    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_port = htons(replyPort_);
    target.sin_addr.s_addr = inet_addr(clientIP_.c_str());
    
    sendto(sockfd_, packet.data(), packet.size(), 0, (struct sockaddr*)&target, sizeof(target));
}
