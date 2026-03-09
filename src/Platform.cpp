#include "../include/Platform.h"
#include <algorithm>
#include <iterator>

#ifdef _WIN32
static bool wsInitialized = false;
#endif

#ifdef _WIN32
#include <windows.h>
#endif

bool PlatformUtils::initNetwork() {
#ifdef _WIN32
    if (!wsInitialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            return false;
        }
        wsInitialized = true;
    }
#endif
    return true;
}

void PlatformUtils::cleanupNetwork() {
#ifdef _WIN32
    if (wsInitialized) {
        WSACleanup();
        wsInitialized = false;
    }
#endif
}

std::string PlatformUtils::getEnv(const char* name) {
    const char* val = getenv(name);
    return val ? std::string(val) : "";
}

bool PlatformUtils::setEnv(const char* name, const char* value) {
#ifdef _WIN32
    return _putenv_s(name, value) == 0;
#else
    return setenv(name, value, 1) == 0;
#endif
}

int PlatformUtils::getPageSize() {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwPageSize;
#else
    return getpagesize();
#endif
}
