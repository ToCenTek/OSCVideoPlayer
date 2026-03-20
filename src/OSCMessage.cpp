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

#include "../include/OSCMessage.h"
#include <cstddef>
#include <cstring>

static std::string readString(const char*& p) {
    const char* start = p;
    std::string s;
    while (*p != '\0') {
        s += *p++;
    }
    p++;
    while ((p - start) % 4 != 0) {
        p++;
    }
    return s;
}

static int32_t readInt32(const char*& p) {
    uint32_t v = ((unsigned char)p[0] << 24) | ((unsigned char)p[1] << 16) | 
                 ((unsigned char)p[2] << 8) | ((unsigned char)p[3]);
    p += 4;
    return (int32_t)v;
}

static float readFloat(const char*& p) {
    union { float f; uint32_t u; } conv;
    conv.u = ((unsigned char)p[0] << 24) | ((unsigned char)p[1] << 16) | 
             ((unsigned char)p[2] << 8) | ((unsigned char)p[3]);
    p += 4;
    return conv.f;
}

OSCMessage parseOSCPacket(const char* data, size_t len) {
    const char* start = data;
    if (len < 8) return OSCMessage();
    
    if (data[0] == '#' && data[1] == 'b' && data[2] == 'u' && data[3] == 'n') {
        OSCBundle bundle;
        const char* p = data + 8;
        while (p - data < (ptrdiff_t)len) {
            if (p + 4 > data + len) break;
            int32_t size = readInt32(p);
            if (size <= 0 || p + size > data + len) break;
            OSCMessage msg = parseOSCPacket(p, size);
            bundle.addMessage(msg);
            p += size;
        }
        return OSCMessage();
    }
    
    OSCMessage msg;
    msg.setAddress(readString(data));
    
    if (*data != ',') return msg;
    data++; // skip comma
    
    std::string types;
    while (*data != '\0' && data < start + len) {
        types += *data++;
    }
    data++; // skip null terminator
    
    // Pad to 4-byte boundary
    while ((data - start) % 4 != 0) {
        data++;
    }
    
    for (char t : types) {
        if (data >= start + len) break;
        switch (t) {
            case 'i': msg.addInt(readInt32(data)); break;
            case 'f': msg.addFloat(readFloat(data)); break;
            case 's': msg.addString(readString(data)); break;
            default: break;
        }
    }
    
    return msg;
}
