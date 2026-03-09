#ifndef PLATFORM_H
#define PLATFORM_H

#include <string>
#include <cstring>
#include <vector>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    
    typedef int socklen_t;
    typedef SOCKET SocketType;
    #define CLOSE_SOCKET closesocket
    #define SOCKET_ERROR_CODE WSAGetLastError()
    
    #define AT_FDCWD -1
    
    #include <windows.h>
    #include <process.h>
    #include <direct.h>
    
    #define mkdir(path, mode) _mkdir(path)
    #define unlink _unlink
    #define strcasecmp _stricmp
    #define strncasecmp _strnicmp
    #define sleep(seconds) Sleep((seconds) * 1000)
    #define usleep(microseconds) Sleep((microseconds) / 1000)
    
    #define F_OK 0
    #define R_OK 4
    #define W_OK 2
    
    #define SIGTERM 15
    #define SIGKILL 9
    #define SIGINT 2
    
    // Note: uint32_t, uint16_t, uint8_t, int32_t, int16_t, int8_t are already defined by stdint.h
    // No need to redefine them
    
    // Windows doesn't have fork, use CreateProcess
    #define fork() 0  // Placeholder - actual implementation needs CreateProcess
    #define setsid() 0
    
    // For getpid
    #define getpid GetCurrentProcessId
    
    // kill implementation for Windows
    inline int kill(int pid, int sig) {
        if (pid <= 0) return -1;
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProcess == NULL) return -1;
        BOOL result = FALSE;
        if (sig == SIGTERM || sig == SIGKILL) {
            result = TerminateProcess(hProcess, 1);
        }
        CloseHandle(hProcess);
        return result ? 0 : -1;
    }
    
    #define PATH_SEP "\\\\"
    
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <dirent.h>
    #include <signal.h>
    #include <sys/wait.h>
    #include <sys/stat.h>
    #include <grp.h>
    #include <pwd.h>
    #include <dlfcn.h>
    #include <pthread.h>
    #include <netdb.h>
    
    typedef int SocketType;
    #define CLOSE_SOCKET close
    #define SOCKET_ERROR_CODE errno
    
    #define PATH_SEP "/"
#endif

// Common cross-platform utilities
class PlatformUtils {
public:
    static bool initNetwork();
    static void cleanupNetwork();
    static std::string getEnv(const char* name);
    static bool setEnv(const char* name, const char* value);
    static int getPageSize();
    
    static std::vector<std::string> globFiles(const std::string& pattern);
};

#endif // PLATFORM_H
