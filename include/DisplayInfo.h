#ifndef DISPLAY_INFO_H
#define DISPLAY_INFO_H

#include <string>
#include <vector>

struct DisplayInfo {
    std::string name;
    std::string identifier;
    int screen = 0;
    int width = 0;
    int height = 0;
    int offsetX = 0;
    int offsetY = 0;
};

std::vector<DisplayInfo> getDisplayInfos();
DisplayInfo findDisplayInfo(const std::string& name);
std::vector<std::string> getDisplays();

#endif
