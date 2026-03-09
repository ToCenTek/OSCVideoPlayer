#include "../include/DisplayInfo.h"
#include "../include/Platform.h"
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

// 获取所有显示器的信息
// 使用Windows API枚举显示器，获取每个显示器的名称、分辨率和位置偏移
std::vector<DisplayInfo> getDisplayInfos() {
    std::vector<DisplayInfo> infos;
    
#ifdef _WIN32
    // 方法1: 使用EnumDisplayDevices枚举所有显示器
    // 这是获取显示器信息的标准Windows API方法
    int i = 0;
    DISPLAY_DEVICE dd;
    dd.cb = sizeof(dd);
    
    while (EnumDisplayDevices(NULL, i, &dd, 0)) {
        if (dd.StateFlags & DISPLAY_DEVICE_ACTIVE) {
            std::string name = dd.DeviceName;
            
            DEVMODE dm;
            dm.dmSize = sizeof(dm);
            if (EnumDisplaySettings(dd.DeviceName, ENUM_CURRENT_SETTINGS, &dm)) {
                infos.push_back({
                    name,
                    dd.DeviceString,
                    i,
                    (int)dm.dmPelsWidth,
                    (int)dm.dmPelsHeight,
                    dm.dmPosition.x,
                    dm.dmPosition.y
                });
            }
        }
        i++;
    }
    
    // 如果成功获取到显示器信息，直接返回
    if (!infos.empty()) {
        return infos;
    }
    
    // 方法2(备用): 使用GetSystemMetrics和MonitorFromPoint获取多显示器信息
    // 当EnumDisplayDevices失败时的备用方案
    for (int j = 0; j < GetSystemMetrics(SM_CMONITORS); j++) {
        RECT rc;
        if (j == 0) {
            // 主显示器
            rc = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
        } else {
            // 其他显示器 - 使用MonitorFromPoint获取
            HMONITOR hMon = MonitorFromPoint({100 * j, 100 * j}, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = {sizeof(mi)};
            if (GetMonitorInfo(hMon, &mi)) {
                rc = mi.rcMonitor;
                infos.push_back({
                    std::string("\\\\.\\DISPLAY") + std::to_string(j + 1),
                    std::string("Monitor ") + std::to_string(j + 1),
                    j,
                    rc.right - rc.left,
                    rc.bottom - rc.top,
                    rc.left,
                    rc.top
                });
            }
        }
    }
    
    if (infos.empty()) {
        int i = 0;
        DISPLAY_DEVICE dd;
        dd.cb = sizeof(dd);
        
        while (EnumDisplayDevices(NULL, i, &dd, 0)) {
            if (dd.StateFlags & DISPLAY_DEVICE_ACTIVE) {
                std::string name = dd.DeviceName;
                std::string identifier = dd.DeviceString;
                
                DEVMODE dm;
                dm.dmSize = sizeof(dm);
                if (EnumDisplaySettings(dd.DeviceName, ENUM_CURRENT_SETTINGS, &dm)) {
                    infos.push_back({
                        name,
                        identifier,
                        i,
                        dm.dmPelsWidth,
                        dm.dmPelsHeight,
                        dm.dmPosition.x,
                        dm.dmPosition.y
                    });
                }
            }
            i++;
        }
    }
#elif __linux__
    {
        FILE* fp = popen("xrandr --listmonitors 2>/dev/null", "r");
        if (fp) {
            char line[1024];
            int idx = 0;
            while (fgets(line, sizeof(line), fp)) {
                char name[256] = {};
                if (sscanf(line, " %d: %255s", &idx, name) >= 2) {
                    infos.push_back({name, name, idx, 0, 0, 0, 0});
                }
            }
            pclose(fp);
        }
        
        if (infos.empty()) {
            fp = popen("xrandr 2>/dev/null", "r");
            if (fp) {
                char line[1024];
                int idx = 0;
                while (fgets(line, sizeof(line), fp)) {
                    char name[256] = {};
                    int w, h, x, y;
                    if (sscanf(line, "%255s connected %dx%d+%d+%d", name, &w, &h, &x, &y) == 5) {
                        infos.push_back({name, name, idx, w, h, x, y});
                        idx++;
                    }
                }
                pclose(fp);
            }
        }
        
        if (infos.empty()) {
            fp = popen("wlr-randr 2>/dev/null", "r");
            if (fp) {
                char line[1024];
                int idx = 0;
                std::string currentName;
                while (fgets(line, sizeof(line), fp)) {
                    int w, h, x, y;
                    if (sscanf(line, "GPU: %*s") == 0 && strlen(line) > 0 && line[0] != ' ') {
                        currentName = line;
                        currentName.erase(currentName.find('\n'));
                    }
                    if (sscanf(line, "%dx%d+%d+%d", &w, &h, &x, &y) == 4) {
                        if (currentName.empty()) {
                            currentName = "HDMI-" + std::to_string(idx);
                        }
                        infos.push_back({currentName, currentName, idx, w, h, x, y});
                        idx++;
                    }
                }
                pclose(fp);
            }
        }
        
        if (infos.empty()) {
            fp = popen("weston-info 2>/dev/null || weston-output-detect 2>/dev/null", "r");
            if (fp) {
                char line[512];
                int idx = 0;
                while (fgets(line, sizeof(line), fp)) {
                    if (strstr(line, "WL_OUTPUT")) {
                        char name[128];
                        if (sscanf(line, "%*[^:]: %127s", name) >= 1) {
                            infos.push_back({name, name, idx, 0, 0, 0, 0});
                            idx++;
                        }
                    }
                }
                pclose(fp);
            }
        }
        
        if (infos.empty()) {
            const char* waylandDisplay = getenv("WAYLAND_DISPLAY");
            if (waylandDisplay) {
                FILE* fp2 = popen("wpctl status 2>/dev/null", "r");
                if (fp2) {
                    char line[256];
                    while (fgets(line, sizeof(line), fp2)) {
                        if (strstr(line, "Screen") || strstr(line, "Display")) {
                            infos.push_back({"primary", "primary", 0, 0, 0, 0, 0});
                            break;
                        }
                    }
                    pclose(fp2);
                }
            }
        }
    }
#elif __APPLE__
    FILE* fp = popen("system_profiler SPDisplaysDataType 2>/dev/null", "r");
    if (fp) {
        char line[512];
        bool inDisplaysSection = false;
        while (fgets(line, sizeof(line), fp)) {
            line[strcspn(line, "\n\r")] = 0;
            
            int leadingSpaces = 0;
            while (leadingSpaces < (int)strlen(line) && line[leadingSpaces] == ' ') leadingSpaces++;
            
            if (strlen(line) > 0 && line[strlen(line)-1] == ':' && leadingSpaces == 0) {
                if (std::string(line).find("Displays") != std::string::npos) {
                    inDisplaysSection = true;
                }
                continue;
            }
            
            if (inDisplaysSection && leadingSpaces >= 8 && strlen(line) > 0 && line[strlen(line)-1] == ':') {
                line[strlen(line)-1] = 0;
                std::string name = line;
                while (!name.empty() && name[0] == ' ') name.erase(name.begin());
                
                if (name.find("Color LCD") != std::string::npos) {
                    name = "Built-in Display";
                }
                
                infos.push_back({name, name, (int)infos.size(), 0, 0, 0, 0});
            }
        }
        pclose(fp);
    }
    
    system("((dns-sd -B _airplay._tcp. > /tmp/airplay_dns.log 2>&1) & sleep 1.5; pkill -f 'dns-sd -B') &");
    
    FILE* fp2 = fopen("/tmp/airplay_dns.log", "r");
    if (fp2) {
        char line[256];
        while (fgets(line, sizeof(line), fp2)) {
            std::string lineStr = line;
            if (lineStr.find("Add") != std::string::npos && lineStr.find("airplay._tcp") != std::string::npos) {
                size_t tcpPos = lineStr.find("_airplay._tcp.");
                if (tcpPos != std::string::npos) {
                    std::string airplayName = lineStr.substr(tcpPos + 15);
                    while (!airplayName.empty() && airplayName[0] == ' ') {
                        airplayName.erase(airplayName.begin());
                    }
                    while (!airplayName.empty() && 
                           (airplayName.back() == '\n' || airplayName.back() == '\r' || airplayName.back() == ' ')) {
                        airplayName.pop_back();
                    }
                    if (!airplayName.empty() && airplayName.find("MacBook") == std::string::npos) {
                        bool exists = false;
                        for (const auto& info : infos) {
                            if (info.name == airplayName) {
                                exists = true;
                                break;
                            }
                        }
                        if (!exists) {
                            infos.push_back({airplayName, airplayName, (int)infos.size(), 0, 0, 0, 0});
                        }
                    }
                }
            }
        }
        fclose(fp2);
    }
#endif
    
    if (infos.empty()) {
        infos.push_back({"default", "default", 0, 0, 0, 0, 0});
    }
    
    return infos;
}

DisplayInfo findDisplayInfo(const std::string& name) {
    auto infos = getDisplayInfos();
    
    // First try numeric index
    int idx = atoi(name.c_str());
    if (idx >= 0 && idx < (int)infos.size()) {
        return infos[idx];
    }
    
    // Try exact match on name or identifier
    for (const auto& info : infos) {
        if (info.name == name || info.identifier == name) {
            return info;
        }
    }
    
    // Try partial match - handle Windows device names like \\.\DISPLAY1
    for (const auto& info : infos) {
        if (info.name.find(name) != std::string::npos || 
            info.identifier.find(name) != std::string::npos) {
            return info;
        }
    }
    
    // Try matching by extracting display number from \\.\DISPLAYN format
    if (name.find("DISPLAY") != std::string::npos) {
        int targetNum = 0;
        size_t pos = name.find("DISPLAY");
        if (pos != std::string::npos) {
            targetNum = atoi(name.c_str() + pos + 7);
        }
        // Find display with matching number in name
        for (const auto& info : infos) {
            if (info.name.find("DISPLAY") != std::string::npos) {
                int num = atoi(info.name.c_str() + info.name.find("DISPLAY") + 7);
                if (num == targetNum) {
                    return info;
                }
            }
        }
    }
    
    return {};
}

std::vector<std::string> getDisplays() {
    std::vector<std::string> displays;
    for (const auto& info : getDisplayInfos()) {
        if (!info.name.empty()) {
            displays.push_back(info.name);
        }
    }
    if (displays.empty()) {
        displays.push_back("default");
    }
    return displays;
}
