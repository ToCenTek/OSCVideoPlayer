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

#include "../include/AudioDevice.h"
#include "../include/Platform.h"
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <propsys.h>
#include <combaseapi.h>
#endif

// 使用mpv获取真实的音频设备列表
std::vector<AudioDevice> getAudioDevices() {
    std::vector<AudioDevice> devices;
    
#ifdef _WIN32
    // 用mpv获取wasapi设备ID列表
    std::vector<std::string> wasapiIDs;
    FILE* fp = popen("mpv --audio-device=help 2>NUL", "r");
    if (fp) {
        char line[512];
        while (fgets(line, sizeof(line), fp)) {
            std::string lineStr = line;
            size_t wasapiPos = lineStr.find("wasapi/");
            if (wasapiPos != std::string::npos) {
                std::string devicePart = lineStr.substr(wasapiPos);
                size_t spacePos = devicePart.find(' ');
                std::string deviceID = devicePart.substr(0, spacePos);
                while (!deviceID.empty() && (deviceID.back() == '\r' || deviceID.back() == '\n' || deviceID.back() == '\'')) {
                    deviceID.pop_back();
                }
                if (!deviceID.empty() && deviceID != "wasapi/default") {
                    wasapiIDs.push_back(deviceID);
                }
            }
        }
        pclose(fp);
    }
    
    // 为每个wasapi设备生成易读的显示名称
    // 根据设备ID的某些特征来判断
    for (size_t i = 0; i < wasapiIDs.size(); i++) {
        std::string deviceID = wasapiIDs[i];
        std::string displayName;
        
        // 根据设备在列表中的位置分配名称
        // 根据用户之前的测试：0是Speakers，1和2是HDMI设备
        if (i == 0) {
            displayName = "Speakers";
        } else {
            displayName = "HDMI-" + std::to_string(i);
        }
        
        devices.push_back({displayName, deviceID});
    }
    
    if (devices.empty()) {
        devices.push_back({"Default", "wasapi/default"});
    }
#elif __linux__
    {
        std::vector<std::string> wasapiIDs;
        FILE* fp = popen("mpv --audio-device=help 2>/dev/null", "r");
        if (fp) {
            char line[512];
            while (fgets(line, sizeof(line), fp)) {
                std::string lineStr = line;
                size_t alsaPos = lineStr.find("alsa/");
                size_t pulsePos = lineStr.find("pulse/");
                size_t pipewirePos = lineStr.find("pipewire/");
                size_t pos = std::string::npos;
                std::string prefix;
                if (pipewirePos != std::string::npos) {
                    pos = pipewirePos;
                    prefix = "pipewire/";
                } else if (pulsePos != std::string::npos) {
                    pos = pulsePos;
                    prefix = "pulse/";
                } else if (alsaPos != std::string::npos) {
                    pos = alsaPos;
                    prefix = "alsa/";
                }
                if (pos != std::string::npos) {
                    std::string devicePart = lineStr.substr(pos);
                    size_t spacePos = devicePart.find(' ');
                    std::string deviceID = devicePart.substr(0, spacePos);
                    while (!deviceID.empty() && (deviceID.back() == '\r' || deviceID.back() == '\n' || deviceID.back() == '\'')) {
                        deviceID.pop_back();
                    }
                    if (!deviceID.empty() && deviceID != prefix + "default" && deviceID != prefix + "auto") {
                        std::string displayName = deviceID.substr(prefix.length());
                        devices.push_back({displayName, deviceID});
                    }
                }
            }
            pclose(fp);
        }
    }
    if (devices.empty()) {
        FILE* fp = popen("pactl list short sinks 2>/dev/null", "r");
        if (fp) {
            char line[1024];
            while (fgets(line, sizeof(line), fp)) {
                char name[256] = {};
                if (sscanf(line, "%*d %255s", name) >= 1) {
                    std::string shortName = name;
                    size_t dotPos = shortName.rfind('.');
                    if (dotPos != std::string::npos) {
                        shortName = shortName.substr(dotPos + 1);
                    }
                    devices.push_back({shortName, name});
                }
            }
            pclose(fp);
        }
    }
    if (devices.empty()) {
        FILE* fp = popen("wpctl list-sinks 2>/dev/null", "r");
        if (fp) {
            char line[512];
            while (fgets(line, sizeof(line), fp)) {
                std::string lineStr = line;
                size_t idPos = lineStr.find(". ");
                if (idPos != std::string::npos) {
                    std::string name = lineStr.substr(idPos + 2);
                    while (!name.empty() && (name.back() == '\r' || name.back() == '\n')) {
                        name.pop_back();
                    }
                    if (!name.empty()) {
                        devices.push_back({name, name});
                    }
                }
            }
            pclose(fp);
        }
    }
    if (devices.empty()) {
        devices.push_back({"Default", "auto"});
    }
#elif __APPLE__
    FILE* fp = popen("mpv --audio-device=help 2>/dev/null", "r");
    if (fp) {
        char line[512];
        while (fgets(line, sizeof(line), fp)) {
            std::string lineStr = line;
            size_t coreaudioPos = lineStr.find("coreaudio/");
            if (coreaudioPos != std::string::npos) {
                std::string devicePart = lineStr.substr(coreaudioPos);
                size_t spacePos = devicePart.find(' ');
                std::string deviceID = devicePart.substr(0, spacePos);
                while (!deviceID.empty() && (deviceID.back() == '\r' || deviceID.back() == '\n' || deviceID.back() == '\'')) {
                    deviceID.pop_back();
                }
                std::string displayName = deviceID.substr(10);
                if (!deviceID.empty() && deviceID != "coreaudio/auto") {
                    devices.push_back({displayName, deviceID});
                }
            }
        }
        pclose(fp);
    }
    
    if (devices.empty()) {
        devices.push_back({"Default", "coreaudio/auto"});
    }
#endif
    
    return devices;
}

std::string getAudioDeviceIDByIndex(int index) {
    // 直接用索引获取设备，避免因设备顺序变化导致的问题
    // 先获取一次设备列表，缓存下来
    static std::vector<AudioDevice> cachedDevices;
    static bool initialized = false;
    
    if (!initialized) {
        cachedDevices = getAudioDevices();
        initialized = true;
    }
    
    if (index >= 0 && index < (int)cachedDevices.size()) {
        return cachedDevices[index].deviceID;
    }
    return "";
}

std::string getAudioDeviceIDByName(const std::string& name) {
    // 先在已缓存的设备列表中查找
    static std::vector<AudioDevice> cachedDevices;
    static bool initialized = false;
    
    if (!initialized) {
        cachedDevices = getAudioDevices();
        initialized = true;
    }
    
    for (const auto& d : cachedDevices) {
        if (d.displayName == name) {
            return d.deviceID;
        }
    }
    
    // 如果name是完整的wasapi设备ID，直接使用
    if (!name.empty()) {
        return name;
    }
    
    return "";
}

std::vector<std::string> getAudioDevicesWithIndex() {
    static std::vector<AudioDevice> cachedDevices;
    static bool initialized = false;
    
    if (!initialized) {
        cachedDevices = getAudioDevices();
        initialized = true;
    }
    
    std::vector<std::string> result;
    for (size_t i = 0; i < cachedDevices.size(); i++) {
        result.push_back(std::to_string(i) + ":" + cachedDevices[i].displayName);
    }
    return result;
}

std::vector<std::string> getAudioDeviceIDs() {
    static std::vector<AudioDevice> cachedDevices;
    static bool initialized = false;
    
    if (!initialized) {
        cachedDevices = getAudioDevices();
        initialized = true;
    }
    
    std::vector<std::string> ids;
    for (const auto& d : cachedDevices) {
        ids.push_back(d.deviceID);
    }
    return ids;
}
