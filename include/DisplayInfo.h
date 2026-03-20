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
