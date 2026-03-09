#include "../include/Player.h"
#include "../include/DisplayInfo.h"
#include "../include/AudioDevice.h"
#include "../include/Platform.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <thread>
#include <deque>
#include <chrono>
#include <cstring>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <set>

#ifndef _WIN32
#include <glob.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#else
// Windows glob replacement - use FindFirstFile/FindNextFile
#include <windows.h>
#endif

#ifdef __APPLE__
#include <libgen.h>
#include <mach-o/dyld.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#endif

std::string getVideoDir() {
#ifdef _WIN32
    char* env = getenv("USERPROFILE");
    if (env) return std::string(env) + "\\Videos";
    return "C:\\Users\\Public\\Videos";
#elif __APPLE__
    char* env = getenv("HOME");
    if (env) return std::string(env) + "/Movies";
    return "/Users/shared/Movies";
#else
    char* env = getenv("HOME");
    if (env) return std::string(env) + "/Videos";
    return "/home/shared/Videos";
#endif
}

std::vector<std::string> getVideoSearchPaths() {
    std::vector<std::string> paths;
    
#ifdef _WIN32
    char* userProfile = getenv("USERPROFILE");
    if (userProfile) {
        std::string home = userProfile;
        paths.push_back(home + "\\Videos");
        paths.push_back(home + "\\Desktop");
        paths.push_back(home + "\\Downloads");
        paths.push_back(home + "\\Documents");
    }
    
    for (char drive = 'D'; drive <= 'K'; drive++) {
        std::string drivePath = std::string(1, drive) + ":\\";
        if (GetDriveTypeA(drivePath.c_str()) != DRIVE_NO_ROOT_DIR) {
            paths.push_back(drivePath + "Videos");
            paths.push_back(drivePath + "Movies");
            paths.push_back(drivePath + "Downloads");
            paths.push_back(drivePath + "Desktop");
        }
    }
#elif __APPLE__
    char* home = getenv("HOME");
    if (home) {
        paths.push_back(std::string(home) + "/Movies");
        paths.push_back(std::string(home) + "/Videos");
        paths.push_back(std::string(home) + "/Downloads");
        paths.push_back(std::string(home) + "/Desktop");
    }
    paths.push_back("/Users/shared/Movies");
    paths.push_back("/Users/shared/Videos");
    
    FILE* fp = popen("ls /Volumes", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            line[strcspn(line, "\n")] = 0;
            if (strlen(line) > 0) {
                paths.push_back(std::string("/Volumes/") + line + "/Movies");
                paths.push_back(std::string("/Volumes/") + line + "/Videos");
                paths.push_back(std::string("/Volumes/") + line + "/Downloads");
            }
        }
        pclose(fp);
    }
#else
    char* home = getenv("HOME");
    if (home) {
        paths.push_back(std::string(home) + "/Videos");
        paths.push_back(std::string(home) + "/Downloads");
        paths.push_back(std::string(home) + "/Desktop");
    }
    paths.push_back("/home/shared/Videos");
    
    FILE* fp = popen("ls /media", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            line[strcspn(line, "\n")] = 0;
            if (strlen(line) > 0) {
                paths.push_back(std::string("/media/") + line);
            }
        }
        pclose(fp);
    }
    
    fp = popen("ls /mnt", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            line[strcspn(line, "\n")] = 0;
            if (strlen(line) > 0) {
                paths.push_back(std::string("/mnt/") + line);
            }
        }
        pclose(fp);
    }
#endif
    
    return paths;
}

bool hasVideoExtension(const std::string& name) {
    size_t pos = name.rfind('.');
    if (pos == std::string::npos) return false;
    std::string ext = name.substr(pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return (ext == "mp4" || ext == "m4v" || ext == "mov" || 
            ext == "avi" || ext == "mkv" || ext == "webm" ||
            ext == "mpg" || ext == "mpeg");
}

void searchVideosInDir(const std::string& dir, int depth, int maxDepth, 
                       std::vector<std::string>& allVideos, std::set<std::string>& seenNames) {
    if (depth > maxDepth) return;
    
    DIR* dp = opendir(dir.c_str());
    if (!dp) return;
    
    struct dirent* entry;
    while ((entry = readdir(dp))) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        
        std::string fullPath = dir + "/" + name;
        
        struct stat st;
        if (stat(fullPath.c_str(), &st) == 0) {
            if (S_ISREG(st.st_mode)) {
                if (hasVideoExtension(name)) {
                    std::string lower = name;
                    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                    if (seenNames.find(lower) == seenNames.end()) {
                        seenNames.insert(lower);
                        allVideos.push_back(fullPath);
                    }
                }
            } else if (S_ISDIR(st.st_mode)) {
                searchVideosInDir(fullPath, depth + 1, maxDepth, allVideos, seenNames);
            }
        }
    }
    closedir(dp);
}

std::vector<std::string> getAllVideos() {
    std::vector<std::string> allVideos;
    std::set<std::string> seenNames;
    
    std::vector<std::string> basePaths = getVideoSearchPaths();
    
    for (const auto& baseDir : basePaths) {
        searchVideosInDir(baseDir, 0, 2, allVideos, seenNames);
    }
    
    return allVideos;
}

std::string findVideoInDir(const std::string& dir, const std::string& filename, int depth = 0) {
    if (depth > 2) return "";
    
    if (hasVideoExtension(filename)) {
        std::string fullPath = dir + PATH_SEP + filename;
#ifdef _WIN32
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA(fullPath.c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            FindClose(hFind);
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                return fullPath;
            }
        }
#else
        glob_t globResult;
        if (glob(fullPath.c_str(), 0, NULL, &globResult) == 0) {
            if (globResult.gl_pathc > 0) {
                std::string found = globResult.gl_pathv[0];
                globfree(&globResult);
                return found;
            }
            globfree(&globResult);
        }
#endif
    } else {
        std::vector<std::string> exts = {"mp4", "m4v", "mov"};
        
        for (const auto& ext : exts) {
            std::string pattern = dir + PATH_SEP + filename + "." + ext;
#ifdef _WIN32
            WIN32_FIND_DATAA fd;
            HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
            if (hFind != INVALID_HANDLE_VALUE) {
                FindClose(hFind);
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    return pattern;
                }
            }
#else
            glob_t globResult;
            if (glob(pattern.c_str(), 0, NULL, &globResult) == 0) {
                if (globResult.gl_pathc > 0) {
                    std::string found = globResult.gl_pathv[0];
                    globfree(&globResult);
                    return found;
                }
                globfree(&globResult);
            }
#endif
        }
    }
    
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA((dir + "\\*").c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return "";
    do {
        std::string name = fd.cFileName;
        if (name == "." || name == "..") continue;
        std::string fullPath = dir + PATH_SEP + name;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            std::string result = findVideoInDir(fullPath, filename, depth + 1);
            if (!result.empty()) {
                FindClose(hFind);
                return result;
            }
        }
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
    return "";
#else
    DIR* dp = opendir(dir.c_str());
    if (!dp) return "";
    
    struct dirent* entry;
    while ((entry = readdir(dp))) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        
        std::string fullPath = dir + "/" + name;
        struct stat st;
        if (stat(fullPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
            std::string result = findVideoInDir(fullPath, filename, depth + 1);
            if (!result.empty()) {
                closedir(dp);
                return result;
            }
        }
    }
    closedir(dp);
    return "";
#endif
}
std::string Player::getSocketPath() {
#ifdef _WIN32
    return "\\\\.\\pipe\\mpv-" + std::to_string(GetCurrentProcessId());
#else
    return "/tmp/mpv-" + std::to_string(getpid()) + ".sock";
#endif
}

Player::Player() {
#ifdef _WIN32
    char exePath[MAX_PATH];
    if (GetModuleFileNameA(NULL, exePath, sizeof(exePath)) > 0) {
        exeDir_ = exePath;
        size_t pos = exeDir_.find_last_of("\\/");
        if (pos != std::string::npos) {
            exeDir_ = exeDir_.substr(0, pos);
        }
    }
#elif __APPLE__
    char exePath[1024];
    uint32_t size = sizeof(exePath);
    if (_NSGetExecutablePath(exePath, &size) == 0) {
        exeDir_ = exePath;
        size_t pos = exeDir_.find_last_of("\\/");
        if (pos != std::string::npos) {
            exeDir_ = exeDir_.substr(0, pos);
        }
    }
#elif __linux__
    char exePath[1024];
    char linkPath[1024];
    snprintf(linkPath, sizeof(linkPath), "/proc/%d/exe", getpid());
    ssize_t len = readlink(linkPath, exePath, sizeof(exePath) - 1);
    if (len > 0) {
        exePath[len] = '\0';
        exeDir_ = exePath;
        size_t pos = exeDir_.find_last_of("\\/");
        if (pos != std::string::npos) {
            exeDir_ = exeDir_.substr(0, pos);
        }
    }
    // 如果获取失败，尝试使用当前工作目录
    if (exeDir_.empty()) {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd))) {
            exeDir_ = cwd;
        }
    }
#endif
}

Player::~Player() {
    stopInternal();
}

std::string Player::findVideo(const std::string& filename) const {
    std::set<std::string> checkedPaths;
    std::vector<std::string> basePaths = getVideoSearchPaths();
    
    for (const auto& baseDir : basePaths) {
        std::deque<std::string> dirs;
        dirs.push_back(baseDir);
        
        while (!dirs.empty()) {
            std::string dir = dirs.front();
            dirs.pop_front();
            
            if (checkedPaths.count(dir)) continue;
            checkedPaths.insert(dir);
            
#ifdef _WIN32
            WIN32_FIND_DATAA fd;
            HANDLE hFind = FindFirstFileA((dir + "\\*").c_str(), &fd);
            if (hFind == INVALID_HANDLE_VALUE) continue;
            
            do {
                std::string name = fd.cFileName;
                if (name == "." || name == "..") continue;
                
                std::string fullPath = dir + "\\" + name;
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    dirs.push_back(fullPath);
                } else if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && hasVideoExtension(name)) {
                    bool nameMatches = (name == filename);
                    if (!nameMatches && !hasVideoExtension(filename)) {
                        nameMatches = (name == filename + ".mp4" || name == filename + ".m4v" || name == filename + ".mov");
                    }
                    
                    if (nameMatches || filename.empty()) {
                        FindClose(hFind);
                        std::cerr << "[findVideo] Found: " << fullPath << std::endl;
                        return fullPath;
                    }
                }
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
#else
            DIR* dp = opendir(dir.c_str());
            if (!dp) continue;
            
            struct dirent* entry;
            while ((entry = readdir(dp))) {
                std::string name = entry->d_name;
                if (name == "." || name == "..") continue;
                
                std::string fullPath = dir + "/" + name;
                struct stat st;
                if (stat(fullPath.c_str(), &st) == 0) {
                    if (S_ISREG(st.st_mode) && hasVideoExtension(name)) {
                        bool nameMatches = (name == filename);
                        if (!nameMatches && !hasVideoExtension(filename)) {
                            nameMatches = (name == filename + ".mp4" || 
                                         name == filename + ".m4v" || 
                                         name == filename + ".mov");
                        }
                        if (nameMatches) {
                            closedir(dp);
                            return fullPath;
                        }
                    } else if (S_ISDIR(st.st_mode)) {
                        dirs.push_back(fullPath);
                    }
                }
            }
            closedir(dp);
#endif
        }
    }
    return "";
}

std::string Player::findHelloVideo() const {
    std::vector<std::string> names = {"hello", "HELLO"};
    for (const auto& name : names) {
        std::string path = findVideo(name);
        if (!path.empty()) return path;
    }
    return "";
}

std::string Player::getHelloVideoName() const {
    return "hello.mp4";
}

void Player::sendMpvCommand(const std::string& cmd) {
    if (socketPath_.empty()) return;
    
#ifdef _WIN32
    HANDLE h = CreateFileA(socketPath_.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }
    
    std::string fullCmd = cmd + "\n";
    DWORD written = 0;
    if (!WriteFile(h, fullCmd.c_str(), fullCmd.size(), &written, NULL)) {
        CloseHandle(h);
        return;
    }
    
    // Read response
    char buf[4096];
    DWORD read = 0;
    ReadFile(h, buf, sizeof(buf) - 1, &read, NULL);
    if (read > 0) {
        buf[read] = 0;
    }
    
    CloseHandle(h);
#else
    int fd = connectToMpv();
    if (fd < 0) return;
    
    std::string fullCmd = cmd + "\n";
    write(fd, fullCmd.c_str(), fullCmd.size());
    close(fd);
#endif
}

int Player::connectToMpv() {
#ifdef _WIN32
    HANDLE h = CreateFileA(socketPath_.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return -1;
    return _open_osfhandle((intptr_t)h, 0);
#else
    struct sockaddr_un addr;
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath_.c_str(), sizeof(addr.sun_path) - 1);
    
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
#endif
}

void Player::cleanupOrphanMpvs() {
#ifdef _WIN32
    FILE* fp = popen("tasklist /FI \"IMAGENAME eq mpv.exe\" /NH", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            int pid = 0;
            if (sscanf(line, "mpv.exe %d", &pid) == 1 || sscanf(line, "mpv%*s %d", &pid) == 1) {
                if (pid > 0 && pid != (int)GetCurrentProcessId()) {
                    kill(pid, SIGTERM);
                    Sleep(50);
                    kill(pid, SIGKILL);
                }
            }
        }
        pclose(fp);
    }
#else
    FILE* fp = popen("pgrep mpv", "r");
    if (fp) {
        char line[64];
        while (fgets(line, sizeof(line), fp)) {
            int pid = atoi(line);
            if (pid > 0 && pid != getpid()) {
                kill(-pid, SIGTERM);
                usleep(50000);
                kill(-pid, SIGKILL);
            }
        }
        pclose(fp);
    }
#endif
}

void Player::Play(const std::string& filename) {
    cleanupOrphanMpvs();
    
    std::lock_guard<std::recursive_mutex> lock(mu_);
    
    if (isPlaying_) {
        isUserStopped_ = true;
        if (!socketPath_.empty()) {
            sendMpvCommand("{\"command\":[\"quit\"]}");
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
        stopInternal();
    }
    
    isPlaying_ = false;
    isPaused_ = false;
    isUserStopped_ = false;
    
    std::string videoPath;
    std::string actualFilename;
    
    bool isHelloVideo = false;
    
    if (filename.empty()) {
        videoPath = findHelloVideo();
        if (!videoPath.empty()) {
            size_t pos = videoPath.find_last_of("/\\");
            actualFilename = videoPath.substr(pos + 1);
        } else {
            actualFilename = "hello.mp4";
            videoPath = findVideo(actualFilename);
        }
        isHelloVideo = true;
        isScreensaver_ = true;
    } else {
        std::string helloName = getHelloVideoName();
        std::string lowerFilename = filename;
        std::transform(lowerFilename.begin(), lowerFilename.end(), lowerFilename.begin(), ::tolower);
        std::string lowerHello = helloName;
        std::transform(lowerHello.begin(), lowerHello.end(), lowerHello.begin(), ::tolower);
        
        if (lowerFilename == lowerHello) {
            isHelloVideo = true;
            isScreensaver_ = true;
        } else {
            isScreensaver_ = false;
        }
        actualFilename = filename;
        videoPath = findVideo(actualFilename);
    }
    
    if (isHelloVideo) {
        if (!loopSetByUser_) {
            loopEnabled_ = true;
        }
    } else {
        if (!loopSetByUser_) {
            loopEnabled_ = false;
        }
    }
    
    if (videoPath.empty()) {
        std::cerr << "Video not found: " << actualFilename << std::endl;
        return;
    }
    
    socketPath_ = getSocketPath();
    
    std::vector<std::string> args;
    args.push_back("--osd-level=0");
    args.push_back("--no-terminal");
    args.push_back("--no-border");
    args.push_back("--no-osc");
    args.push_back("--no-input-default-bindings");
    args.push_back("--cursor-autohide=1000");
    
    if (!audioDevice_.empty()) {
#ifdef __APPLE__
        if (audioDevice_ == "Default" || audioDevice_ == "coreaudio" || audioDevice_ == "coreaudio/auto") {
            args.push_back("--audio-device=coreaudio");
        } else if (audioDevice_.find("coreaudio/") == 0) {
            args.push_back("--audio-device=" + audioDevice_);
        } else {
            args.push_back("--audio-device=coreaudio/" + audioDevice_);
        }
#elif _WIN32
        std::string device = audioDevice_;
        if (device.find("wasapi/") != 0) {
            device = "wasapi/" + device;
        }
        args.push_back("--audio-device=" + device);
        std::cout << "[Audio] Using audio device: " << device << std::endl;
#elif __linux__
        if (!audioDevice_.empty()) {
            args.push_back("--audio-device=" + audioDevice_);
            std::cout << "[Audio] Using audio device: " << audioDevice_ << std::endl;
        }
#else
        args.push_back("--audio-device=" + audioDevice_);
        std::cout << "[Audio] Using audio device: " << audioDevice_ << std::endl;
#endif
    }
    
    if (!displayName_.empty()) {
        auto info = findDisplayInfo(displayName_);
        if (!info.name.empty()) {
#ifdef _WIN32
            // For Windows, use --fs-screen to select monitor for fullscreen
            // and --geometry to position window before fullscreen
            args.push_back("--fs-screen=" + std::to_string(info.screen));
            if (info.width > 0 && info.height > 0) {
                std::ostringstream oss;
                oss << info.width << "x" << info.height << "+" << info.offsetX << "+" << info.offsetY;
                args.push_back("--geometry=" + oss.str());
                std::cout << "[Display] Using geometry: " << oss.str() << std::endl;
            }
            std::cout << "[Display] Target screen: " << info.screen << " (" << info.name << ")" << std::endl;
#elif defined(__linux__)
            if (info.width > 0 && info.height > 0) {
                std::ostringstream oss;
                oss << info.width << "x" << info.height << "+" << info.offsetX << "+" << info.offsetY;
                args.push_back("--geometry=" + oss.str());
                std::cout << "[Display] Using geometry: " << oss.str() << std::endl;
            }
            args.push_back("--x11-name=mpv-" + info.name);
#else
            args.push_back("--screen=" + std::to_string(info.screen));
#endif
            std::cout << "[Display] Switching to screen " << info.screen << ": " << info.name << std::endl;
        }
    }
    
    if (loopEnabled_) {
        args.push_back("--loop=inf");
        args.push_back("--keep-open=yes");
    }
    
    // Default fullscreen based on saved mode (must be after screen/geometry)
    if (fullscreenMode_ == 3) {
        args.push_back("--fullscreen");
    }
    
    args.push_back("--input-ipc-server=" + socketPath_);
    
#ifdef __APPLE__
    if (!exeDir_.empty()) {
        args.push_back("--script=" + exeDir_ + "/frame_timing.lua");
    } else {
        args.push_back("--script=frame_timing.lua");
    }
#elif __linux__
    if (!exeDir_.empty()) {
        args.push_back("--script=" + exeDir_ + "/frame_timing.lua");
    } else {
        args.push_back("--script=frame_timing.lua");
    }
#elif _WIN32
    if (!exeDir_.empty()) {
        args.push_back("--script=" + exeDir_ + "\\frame_timing.lua");
    } else {
        args.push_back("--script=frame_timing.lua");
    }
#endif
    
    args.push_back(videoPath);
    
#ifdef _WIN32
    // Find mpv executable
    std::string mpvPath = "";
    const char* mpvEnv = getenv("MPV_PATH");
    if (mpvEnv && strlen(mpvEnv) > 0) {
        mpvPath = mpvEnv;
    } else {
        // Try using where.exe to find mpv
        FILE* fp = popen("where mpv 2>NUL", "r");
        if (fp) {
            char line[MAX_PATH];
            if (fgets(line, sizeof(line), fp)) {
                // Remove newline
                line[strcspn(line, "\r\n")] = 0;
                if (strlen(line) > 0) {
                    mpvPath = line;
                }
            }
            pclose(fp);
        }
        
        // If not found, check common locations
        if (mpvPath.empty()) {
            char* userProfile = getenv("USERPROFILE");
            char* programFiles = getenv("PROGRAMFILES");
            char* programFilesX86 = getenv("PROGRAMFILES(X86)");
            char* appData = getenv("APPDATA");
            char* localAppData = getenv("LOCALAPPDATA");
            // Skip download path, we'll add it directly below
            
            std::vector<std::string> searchPaths;
            if (programFiles) searchPaths.push_back(std::string(programFiles) + "\\mpv\\mpv.exe");
            if (programFilesX86) searchPaths.push_back(std::string(programFilesX86) + "\\mpv\\mpv.exe");
            if (appData) searchPaths.push_back(std::string(appData) + "\\mpv\\mpv.exe");
            if (localAppData) {
                searchPaths.push_back(std::string(localAppData) + "\\mpv\\mpv.exe");
                searchPaths.push_back(std::string(localAppData) + "\\Programs\\mpv\\mpv.exe");
            }
            if (userProfile) {
                searchPaths.push_back(std::string(userProfile) + "\\mpv\\mpv.exe");
                searchPaths.push_back(std::string(userProfile) + "\\Downloads\\mpv\\mpv.exe");
            }
            searchPaths.push_back("C:\\mpv\\mpv.exe");
            searchPaths.push_back("D:\\mpv\\mpv.exe");
            searchPaths.push_back("E:\\mpv\\mpv.exe");
            searchPaths.push_back(".\\mpv.exe");
            searchPaths.push_back(".\\portable\\mpv.exe");
            searchPaths.push_back(".\\bin\\mpv.exe");
            searchPaths.push_back(".\\tools\\mpv.exe");
            
            for (const auto& p : searchPaths) {
                WIN32_FIND_DATAA fd;
                if (FindFirstFileA(p.c_str(), &fd) != INVALID_HANDLE_VALUE) {
                    mpvPath = p;
                    break;
                }
            }
        }
    }
    
    // Replace .com with .exe if needed
    if (!mpvPath.empty()) {
        size_t pos = mpvPath.rfind(".com");
        if (pos != std::string::npos && pos == mpvPath.length() - 4) {
            mpvPath = mpvPath.substr(0, pos) + ".exe";
        }
    }
    
    if (mpvPath.empty()) {
        std::cerr << "Error: mpv not found! Please install mpv." << std::endl;
        return;
    }
    
    // Build command line string
    std::string cmdLine = "\"" + mpvPath + "\"";
    for (auto& a : args) {
        cmdLine += " \"" + a + "\"";
    }
    
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessA(mpvPath.c_str(), const_cast<LPSTR>(cmdLine.c_str()), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        DWORD error = GetLastError();
        LPSTR errorMsg = nullptr;
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error, 0, (LPSTR)&errorMsg, 0, NULL);
        std::cerr << "Failed to start mpv: " << (errorMsg ? errorMsg : "unknown error") << std::endl;
        if (errorMsg) LocalFree(errorMsg);
        return;
    }
    childPid_ = pi.dwProcessId;
    CloseHandle(pi.hThread);
#else
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>("mpv"));
    for (auto& a : args) {
        argv.push_back(const_cast<char*>(a.c_str()));
    }
    argv.push_back(nullptr);
    
    for (size_t i = 0; argv[i]; i++) {
        // debug output removed
    }
    
#ifndef _WIN32
    setenv("PIPEWIRE_QUIET", "1", 1);
    setenv("PIPEWIRE_DEBUG", "0", 1);
#endif
    
    childPid_ = fork();
    if (childPid_ == 0) {
        setpgid(0, 0);
        execvp("mpv", argv.data());
        _exit(1);
    } else if (childPid_ < 0) {
        std::cerr << "Failed to fork mpv" << std::endl;
        return;
    }
#endif
    
    isPlaying_ = true;
    isPaused_ = false;
    currentFile_ = actualFilename;
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    sendMpvCommand("{\"command\":[\"set_property\",\"fullscreen\",true]}");
    
    if (tctEnabled_ && !tctText_.empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ShowText(tctText_, tctFontSize_, true, tctPosition_);
    }
    
    // 恢复FPS显示（如果之前已开启）
    if (fpsDisplayMode_ > 0) {
        // 先停止之前的线程
        if (fpsDisplayRunning_) {
            fpsDisplayRunning_ = false;
            if (fpsDisplayThread_.joinable()) {
                fpsDisplayThread_.detach();
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        fpsDisplayRunning_ = true;
        sendMpvCommand("{\"command\":[\"set_property\",\"osd-level\",3]}");
        sendMpvCommand("{\"command\":[\"set_property\",\"osd-font-size\",48]}");
        
        fpsDisplayThread_ = std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            while (fpsDisplayRunning_) {
                {
                    std::lock_guard<std::recursive_mutex> lock(mu_);
                    if (fpsDisplayMode_ > 0 && !socketPath_.empty()) {
                        std::string timing = GetFrameTiming();
                        double last = 0, avg = 0, peak = 0;
                        if (sscanf(timing.c_str(), "%lf/%lf/%lf", &last, &avg, &peak) == 3 && last > 0) {
                            char osdText[256];
                            snprintf(osdText, sizeof(osdText), "Last:%dus Avg:%dus Peak:%dus", (int)last, (int)avg, (int)peak);
                            std::string cmd = "{\"command\":[\"show-text\",\"" + std::string(osdText) + "\",1000]}";
                            sendMpvCommand(cmd);
                        }
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        });
    }
}

void Player::stopInternal() {
#ifdef _WIN32
    if (childPid_ > 0) {
        HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, childPid_);
        if (h) {
            TerminateProcess(h, 0);
            CloseHandle(h);
        }
        childPid_ = -1;
    }
#else
    if (childPid_ > 0) {
        kill(childPid_, SIGTERM);
        usleep(100000);
        kill(childPid_, SIGKILL);
        waitpid(childPid_, nullptr, 0);
        childPid_ = -1;
    }
#endif
    
    if (!socketPath_.empty()) {
#ifndef _WIN32
        unlink(socketPath_.c_str());
#endif
        socketPath_.clear();
    }
    
    isPlaying_ = false;
    isPaused_ = false;
    currentFile_.clear();
    
    int savedFpsMode = fpsDisplayMode_;
    fpsDisplayRunning_ = false;
    if (fpsDisplayThread_.joinable()) {
        fpsDisplayThread_.detach();
    }
    fpsDisplayMode_ = savedFpsMode;
}

void Player::Stop() {
    std::lock_guard<std::recursive_mutex> lock(mu_);
    isUserStopped_ = true;
    stopInternal();
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    std::string helloPath = findHelloVideo();
    if (!helloPath.empty()) {
        Play("");
    }
}

void Player::StopExit() {
    std::lock_guard<std::recursive_mutex> lock(mu_);
    isUserStopped_ = true;
    stopInternal();
}

void Player::Pause() {
    std::lock_guard<std::recursive_mutex> lock(mu_);
    if (!isPlaying_ || socketPath_.empty()) return;
    
    std::string cmd = isPaused_ ? 
        "{\"command\":[\"set_property\",\"pause\",false]}" :
        "{\"command\":[\"set_property\",\"pause\",true]}";
    sendMpvCommand(cmd);
    isPaused_ = !isPaused_;
}

bool Player::IsPaused() const {
    return isPaused_;
}

void Player::SetVolume(int vol) {
    std::lock_guard<std::recursive_mutex> lock(mu_);
    volume_ = vol;
    sendMpvCommand("{\"command\":[\"set_property\",\"volume\"," + std::to_string(vol) + "]}");
}

void Player::SetLoop(bool enable) {
    std::lock_guard<std::recursive_mutex> lock(mu_);
    loopSetByUser_ = true;
    loopEnabled_ = enable;
    sendMpvCommand("{\"command\":[\"set_property\",\"loop\",\"" + std::string(enable ? "inf" : "no") + "\"]}");
}

void Player::SetFullscreen(int mode) {
    std::lock_guard<std::recursive_mutex> lock(mu_);
    fullscreenMode_ = mode;
    if (socketPath_.empty()) return;
    
    // 先取消全屏
    sendMpvCommand("{\"command\":[\"set_property\",\"fullscreen\",false]}");
    
    if (mode == -1) {
        sendMpvCommand("{\"command\":[\"cycle\",\"fullscreen\"]}");
    } else if (mode == 0) {
        // 窗口模式 - 重新应用geometry保持在当前显示器
        if (!displayName_.empty()) {
            auto info = findDisplayInfo(displayName_);
            if (!info.name.empty() && info.width > 0 && info.height > 0) {
                char geoCmd[256];
                snprintf(geoCmd, sizeof(geoCmd), "{\"command\":[\"set_property\",\"geometry\",\"%dx%d+%d+%d\"]}", 
                    info.width, info.height, info.offsetX, info.offsetY);
                sendMpvCommand(geoCmd);
            }
        }
        sendMpvCommand("{\"command\":[\"set_property\",\"keepaspect\",\"yes\"]}");
        sendMpvCommand("{\"command\":[\"set_property\",\"panscan\",0.0]}");
        sendMpvCommand("{\"command\":[\"set_property\",\"video-zoom\",0]}");
        sendMpvCommand("{\"command\":[\"set_property\",\"scale\",1.0]}");
        sendMpvCommand("{\"command\":[\"set_property\",\"scale-x\",1.0]}");
        sendMpvCommand("{\"command\":[\"set_property\",\"scale-y\",1.0]}");
    } else if (mode == 1) {
        // 全屏
        sendMpvCommand("{\"command\":[\"set_property\",\"keepaspect\",\"no\"]}");
        sendMpvCommand("{\"command\":[\"set_property\",\"fullscreen\",true]}");
    } else if (mode == 2) {
        // 左右拉伸（填充整个宽度）
        sendMpvCommand("{\"command\":[\"set_property\",\"keepaspect\",\"no\"]}");
        sendMpvCommand("{\"command\":[\"set_property\",\"panscan\",0.0]}");
        sendMpvCommand("{\"command\":[\"set_property\",\"video-zoom\",0]}");
        sendMpvCommand("{\"command\":[\"set_property\",\"scale\",1.0]}");
        sendMpvCommand("{\"command\":[\"set_property\",\"scale-x\",1.0]}");
        sendMpvCommand("{\"command\":[\"set_property\",\"scale-y\",0]}");
    } else if (mode == 3) {
        // 上下拉伸（填充整个高度）
        sendMpvCommand("{\"command\":[\"set_property\",\"keepaspect\",\"no\"]}");
        sendMpvCommand("{\"command\":[\"set_property\",\"panscan\",0.0]}");
        sendMpvCommand("{\"command\":[\"set_property\",\"video-zoom\",0]}");
        sendMpvCommand("{\"command\":[\"set_property\",\"scale\",1.0]}");
        sendMpvCommand("{\"command\":[\"set_property\",\"scale-x\",0]}");
        sendMpvCommand("{\"command\":[\"set_property\",\"scale-y\",1.0]}");
    }
}

std::string Player::GetCurrentFile() const {
    return currentFile_;
}

double Player::GetPosition() {
    std::lock_guard<std::recursive_mutex> lock(mu_);
    if (socketPath_.empty()) return 0;
    
    int fd = connectToMpv();
    if (fd < 0) return 0;
    
    std::string cmd = "{\"command\":[\"get_property\",\"percent-pos\"]}\n";
    write(fd, cmd.c_str(), cmd.size());
    
    char buf[1024] = {};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    struct timeval tv = {2, 0};
    if (select(fd + 1, &fds, NULL, NULL, &tv) > 0) {
        read(fd, buf, sizeof(buf));
    }
    close(fd);
    
    double pos = 0;
    const char* p = strstr(buf, "\"data\":");
    if (p) {
        sscanf(p + 7, "%lf", &pos);
    }
    return pos;
}

double Player::GetTime() {
    std::lock_guard<std::recursive_mutex> lock(mu_);
    if (socketPath_.empty()) return 0;
    
#ifdef _WIN32
    HANDLE h = CreateFileA(socketPath_.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    
    std::string cmd = "{\"command\":[\"get_property\",\"time-pos\"]}\n";
    DWORD written = 0;
    WriteFile(h, cmd.c_str(), cmd.size(), &written, NULL);
    
    char buf[1024] = {};
    DWORD read = 0;
    if (WaitForSingleObject(h, 2000) == WAIT_OBJECT_0) {
        ReadFile(h, buf, sizeof(buf) - 1, &read, NULL);
    }
    CloseHandle(h);
    
    double time = 0;
    const char* p = strstr(buf, "\"data\":");
    if (p) {
        sscanf(p + 7, "%lf", &time);
    }
    return time;
#else
    int fd = connectToMpv();
    if (fd < 0) return 0;
    
    std::string cmd = "{\"command\":[\"get_property\",\"time-pos\"]}\n";
    write(fd, cmd.c_str(), cmd.size());
    
    char buf[1024] = {};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    struct timeval tv = {2, 0};
    if (select(fd + 1, &fds, NULL, NULL, &tv) > 0) {
        read(fd, buf, sizeof(buf));
    }
    close(fd);
    
    double time = 0;
    const char* p = strstr(buf, "\"data\":");
    if (p) {
        sscanf(p + 7, "%lf", &time);
    }
    return time;
#endif
}

double Player::GetFPS() {
    std::lock_guard<std::recursive_mutex> lock(mu_);
    if (socketPath_.empty()) return 0;
    
#ifdef _WIN32
    HANDLE h = CreateFileA(socketPath_.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    
    std::string cmd = "{\"command\":[\"get_property\",\"estimated-vf-fps\"]}\n";
    DWORD written = 0;
    WriteFile(h, cmd.c_str(), cmd.size(), &written, NULL);
    
    char buf[1024] = {};
    DWORD read = 0;
    if (WaitForSingleObject(h, 2000) == WAIT_OBJECT_0) {
        ReadFile(h, buf, sizeof(buf) - 1, &read, NULL);
    }
    CloseHandle(h);
    
    double fps = 0;
    const char* p = strstr(buf, "\"data\":");
    if (p) {
        sscanf(p + 7, "%lf", &fps);
    }
    
    if (fps <= 0) {
        h = CreateFileA(socketPath_.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            cmd = "{\"command\":[\"get_property\",\"video-dec-params/fps\"]}\n";
            WriteFile(h, cmd.c_str(), cmd.size(), &written, NULL);
            read = 0;
            if (WaitForSingleObject(h, 2000) == WAIT_OBJECT_0) {
                ReadFile(h, buf, sizeof(buf) - 1, &read, NULL);
            }
            CloseHandle(h);
            p = strstr(buf, "\"data\":");
            if (p) {
                sscanf(p + 7, "%lf", &fps);
            }
        }
    }
    
    return fps;
#else
    int fd = connectToMpv();
    if (fd < 0) return 0;
    
    std::string cmd = "{\"command\":[\"get_property\",\"estimated-vf-fps\"]}\n";
    write(fd, cmd.c_str(), cmd.size());
    
    char buf[1024] = {};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    struct timeval tv = {2, 0};
    if (select(fd + 1, &fds, NULL, NULL, &tv) > 0) {
        read(fd, buf, sizeof(buf));
    }
    close(fd);
    
    double fps = 0;
    const char* p = strstr(buf, "\"data\":");
    if (p) {
        sscanf(p + 7, "%lf", &fps);
    }
    
    if (fps <= 0) {
        fd = connectToMpv();
        if (fd >= 0) {
            cmd = "{\"command\":[\"get_property\",\"video-dec-params/fps\"]}\n";
            write(fd, cmd.c_str(), cmd.size());
            FD_ZERO(&fds);
            FD_SET(fd, &fds);
            tv.tv_sec = 2;
            if (select(fd + 1, &fds, NULL, NULL, &tv) > 0) {
                read(fd, buf, sizeof(buf));
            }
            close(fd);
            p = strstr(buf, "\"data\":");
            if (p) {
                sscanf(p + 7, "%lf", &fps);
            }
        }
    }
    
    return fps;
#endif
}

double Player::GetDuration() {
    std::lock_guard<std::recursive_mutex> lock(mu_);
    if (socketPath_.empty()) return 0;
    
#ifdef _WIN32
    HANDLE h = CreateFileA(socketPath_.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    
    std::string cmd = "{\"command\":[\"get_property\",\"duration\"]}\n";
    DWORD written = 0;
    WriteFile(h, cmd.c_str(), cmd.size(), &written, NULL);
    
    char buf[1024] = {};
    DWORD read = 0;
    if (WaitForSingleObject(h, 2000) == WAIT_OBJECT_0) {
        ReadFile(h, buf, sizeof(buf) - 1, &read, NULL);
    }
    CloseHandle(h);
    
    double duration = 0;
    const char* p = strstr(buf, "\"data\":");
    if (p) {
        sscanf(p + 7, "%lf", &duration);
    }
    return duration;
#else
    int fd = connectToMpv();
    if (fd < 0) return 0;
    
    std::string cmd = "{\"command\":[\"get_property\",\"duration\"]}\n";
    write(fd, cmd.c_str(), cmd.size());
    
    char buf[1024] = {};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    struct timeval tv = {2, 0};
    if (select(fd + 1, &fds, NULL, NULL, &tv) > 0) {
        read(fd, buf, sizeof(buf));
    }
    close(fd);
    
    double duration = 0;
    const char* p = strstr(buf, "\"data\":");
    if (p) {
        sscanf(p + 7, "%lf", &duration);
    }
    return duration;
#endif
}

std::string Player::GetFrameTiming() {
    if (socketPath_.empty()) return "0/0/0/0";
    
#ifdef _WIN32
    HANDLE h = CreateFileA(socketPath_.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return "0/0/0/0";
    
    std::string cmd = "{\"command\":[\"get_property\",\"estimated-vf-fps\"]}\n";
    DWORD written = 0;
    WriteFile(h, cmd.c_str(), cmd.size(), &written, NULL);
    
    char buf[1024] = {};
    DWORD read = 0;
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadTotalTimeoutConstant = 100;
    SetCommTimeouts(h, &timeouts);
    ReadFile(h, buf, sizeof(buf) - 1, &read, NULL);
    CloseHandle(h);
    buf[read > 0 ? read : 0] = 0;
    
    double fps = 0;
    const char* p = strstr(buf, "\"data\":");
    if (p) {
        sscanf(p + 7, "%lf", &fps);
    }
    
    if (fps > 0) {
        double frame_us = 1000000.0 / fps;
        char result[128];
        snprintf(result, sizeof(result), "%.0f/%.0f/%.0f", frame_us, frame_us, frame_us);
        return result;
    }
    
    return "0/0/0/0";
#else
    int fd = connectToMpv();
    if (fd < 0) return "0/0/0/0";
    
    std::string cmd = "{\"command\":[\"get_property\",\"estimated-vf-fps\"]}\n";
    write(fd, cmd.c_str(), cmd.size());
    
    char buf[1024] = {};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    struct timeval tv = {0, 100000};
    if (select(fd + 1, &fds, NULL, NULL, &tv) > 0) {
        read(fd, buf, sizeof(buf));
    }
    close(fd);
    
    double fps = 0;
    const char* p = strstr(buf, "\"data\":");
    if (p) {
        sscanf(p + 7, "%lf", &fps);
    }
    
    if (fps > 0) {
        double frame_us = 1000000.0 / fps;
        char result[128];
        snprintf(result, sizeof(result), "%.0f/%.0f/%.0f", frame_us, frame_us, frame_us);
        return result;
    }
    
    return "0/0/0/0";
#endif
}

void Player::SetFPSDisplay(int mode) {
    std::lock_guard<std::recursive_mutex> lock(mu_);
    
    if (mode == 0) {
        fpsDisplayRunning_ = false;
        fpsDisplayMode_ = 0;
        if (fpsDisplayThread_.joinable()) {
            fpsDisplayThread_.detach();
        }
        sendMpvCommand("{\"command\":[\"show-text\",\"\",0]}");
        sendMpvCommand("{\"command\":[\"set_property\",\"osd-level\",0]}");
        return;
    }
    
    if (fpsDisplayThread_.joinable()) {
        fpsDisplayRunning_ = false;
        fpsDisplayThread_.detach();
    }
    
    fpsDisplayMode_ = mode;
    fpsDisplayRunning_ = true;
    
    sendMpvCommand("{\"command\":[\"set_property\",\"osd-level\",3]}");
    sendMpvCommand("{\"command\":[\"set_property\",\"osd-font-size\",48]}");
    
    fpsDisplayThread_ = std::thread([this]() {
        while (true) {
            int modeCopy = 0;
            {
                std::lock_guard<std::recursive_mutex> lock(mu_);
                modeCopy = fpsDisplayMode_;
                if (!fpsDisplayRunning_ || modeCopy == 0) break;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            std::string timing;
            double last = 0, avg = 0, peak = 0;
            {
                std::lock_guard<std::recursive_mutex> lock(mu_);
                if (!socketPath_.empty() && fpsDisplayRunning_) {
                    timing = GetFrameTiming();
                    sscanf(timing.c_str(), "%lf/%lf/%lf", &last, &avg, &peak);
                }
            }
            
            {
                std::lock_guard<std::recursive_mutex> lock(mu_);
                if (!fpsDisplayRunning_ || fpsDisplayMode_ == 0) break;
            }
            
            if (last > 0) {
                char osdText[256];
                if (modeCopy == 2) {
                    double last_fps = 1000000.0 / last;
                    double avg_fps = 1000000.0 / avg;
                    double peak_fps = 1000000.0 / peak;
                    snprintf(osdText, sizeof(osdText), "Last:%.6fHz Avg:%.6fHz Peak:%.6fHz", last_fps, avg_fps, peak_fps);
                } else {
                    snprintf(osdText, sizeof(osdText), "Last:%.0fus Avg:%.0fus Peak:%.0fus", last, avg, peak);
                }
                std::string cmd = "{\"command\":[\"show-text\",\"" + std::string(osdText) + "\",999999]}";
                sendMpvCommand(cmd);
            }
        }
    });
}

void Player::SeekToPercent(double percent) {
    std::lock_guard<std::recursive_mutex> lock(mu_);
    if (!isPlaying_ || socketPath_.empty()) return;
    
    sendMpvCommand("{\"command\":[\"seek\"," + std::to_string(percent) + ",\"absolute-percent\"]}");
}

void Player::SeekToFrame(int frame) {
    std::lock_guard<std::recursive_mutex> lock(mu_);
    if (!isPlaying_ || socketPath_.empty()) return;
    
    double fps = GetFPS();
    if (fps > 0) {
        if (frame >= 0) {
            double seconds = frame / fps;
            sendMpvCommand("{\"command\":[\"seek\"," + std::to_string(seconds) + ",\"absolute\"]}");
        } else {
            double duration = GetDuration();
            if (duration > 0) {
                double seconds = duration - ((-frame) / fps);
                if (seconds < 0) seconds = 0;
                sendMpvCommand("{\"command\":[\"seek\"," + std::to_string(seconds) + ",\"absolute\"]}");
            }
        }
    }
}

void Player::SeekToTime(double seconds) {
    std::lock_guard<std::recursive_mutex> lock(mu_);
    if (!isPlaying_ || socketPath_.empty()) return;
    
    sendMpvCommand("{\"command\":[\"seek\"," + std::to_string(seconds) + ",\"absolute\"]}");
}

void Player::SetAudioDevice(const std::string& device) {
    std::lock_guard<std::recursive_mutex> lock(mu_);
    audioDevice_ = device;
    
    if (isPlaying_ && !currentFile_.empty()) {
        std::string current = currentFile_;
        bool loop = loopEnabled_;
        
#ifdef _WIN32
        // Windows: 使用命名管道发送quit命令
        if (!socketPath_.empty()) {
            HANDLE h = CreateFileA(socketPath_.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
            if (h != INVALID_HANDLE_VALUE) {
                std::string cmd = "{\"command\":[\"quit\"]}\n";
                DWORD written = 0;
                WriteFile(h, cmd.c_str(), cmd.size(), &written, NULL);
                CloseHandle(h);
            }
        }
#else
        if (!socketPath_.empty()) {
            int fd = connectToMpv();
            if (fd >= 0) {
                std::string cmd = "{\"command\":[\"quit\"]}\n";
                write(fd, cmd.c_str(), cmd.size());
                close(fd);
            }
        }
#endif
        
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        stopInternal();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        std::string videoPath = findVideo(current);
        if (!videoPath.empty()) {
            socketPath_ = getSocketPath();
            
            std::vector<std::string> args;
            args.push_back("--osd-level=0");
            args.push_back("--no-terminal");
            args.push_back("--no-border");
            args.push_back("--no-osc");
            args.push_back("--no-input-default-bindings");
            args.push_back("--cursor-autohide=1000");
            
#ifdef __APPLE__
            if (!audioDevice_.empty()) {
                if (audioDevice_ == "Default" || audioDevice_ == "coreaudio" || audioDevice_ == "coreaudio/auto") {
                    args.push_back("--audio-device=coreaudio");
                } else if (audioDevice_.find("coreaudio/") == 0) {
                    args.push_back("--audio-device=" + audioDevice_);
                } else {
                    args.push_back("--audio-device=coreaudio/" + audioDevice_);
                }
            }
            if (!displayName_.empty()) {
                int screenNum = atoi(displayName_.c_str());
                if (screenNum >= 0) {
                    args.push_back("--screen=" + std::to_string(screenNum));
                }
            }
#elif __linux__
            if (!audioDevice_.empty()) {
                std::string device = audioDevice_;
                if (device.find("pulse/") != 0 && device.find("alsa/") != 0 && 
                    device.find("pipewire/") != 0 && device.find("coreaudio/") != 0) {
                    device = "auto";
                }
                args.push_back("--audio-device=" + device);
                std::cout << "[Audio] Using audio device: " << device << std::endl;
            }
            if (!displayName_.empty()) {
                auto info = findDisplayInfo(displayName_);
                if (!info.name.empty()) {
                    if (info.width > 0 && info.height > 0) {
                        std::ostringstream oss;
                        oss << info.width << "x" << info.height << "+" << info.offsetX << "+" << info.offsetY;
                        args.push_back("--geometry=" + oss.str());
                    }
                    args.push_back("--x11-name=mpv-" + info.name);
                }
            }
#elif _WIN32
            if (!audioDevice_.empty()) {
                std::string device = audioDevice_;
                if (device.find("wasapi/") != 0) {
                    device = "wasapi/" + device;
                }
                args.push_back("--audio-device=" + device);
                std::cout << "[Audio] Using audio device: " << device << std::endl;
            }
            if (!displayName_.empty()) {
                auto info = findDisplayInfo(displayName_);
                if (!info.name.empty()) {
                    if (info.width > 0 && info.height > 0) {
                        std::ostringstream oss;
                        oss << info.width << "x" << info.height << "+" << info.offsetX << "+" << info.offsetY;
                        args.push_back("--geometry=" + oss.str());
                    }
                    args.push_back("--fs-screen=" + std::to_string(info.screen));
                }
            }
#endif
            
            if (loop) {
                args.push_back("--loop=inf");
                args.push_back("--keep-open=yes");
            }
            
            args.push_back("--input-ipc-server=" + socketPath_);
#ifdef __APPLE__
            if (!exeDir_.empty()) {
                args.push_back("--script=" + exeDir_ + "/frame_timing.lua");
            } else {
                args.push_back("--script=frame_timing.lua");
            }
#elif __linux__
            if (!exeDir_.empty()) {
                args.push_back("--script=" + exeDir_ + "/frame_timing.lua");
            } else {
                args.push_back("--script=frame_timing.lua");
            }
#elif _WIN32
            if (!exeDir_.empty()) {
                args.push_back("--script=" + exeDir_ + "\\frame_timing.lua");
            } else {
                args.push_back("--script=frame_timing.lua");
            }
#endif
            args.push_back(videoPath);
            
#ifdef _WIN32
            // Windows: 使用CreateProcess启动mpv
            std::string mpvPath = "";
            const char* mpvEnv = getenv("MPV_PATH");
            if (mpvEnv && strlen(mpvEnv) > 0) {
                mpvPath = mpvEnv;
            } else {
                FILE* fp = popen("where mpv 2>NUL", "r");
                if (fp) {
                    char line[MAX_PATH];
                    if (fgets(line, sizeof(line), fp)) {
                        line[strcspn(line, "\r\n")] = 0;
                        if (strlen(line) > 0) {
                            mpvPath = line;
                        }
                    }
                    pclose(fp);
                }
                
                if (mpvPath.empty()) {
                    char* userProfile = getenv("USERPROFILE");
                    char* programFiles = getenv("PROGRAMFILES");
                    char* programFilesX86 = getenv("PROGRAMFILES(X86)");
                    char* appData = getenv("APPDATA");
                    char* localAppData = getenv("LOCALAPPDATA");
                    
                    std::vector<std::string> searchPaths;
                    if (programFiles) searchPaths.push_back(std::string(programFiles) + "\\mpv\\mpv.exe");
                    if (programFilesX86) searchPaths.push_back(std::string(programFilesX86) + "\\mpv\\mpv.exe");
                    if (appData) searchPaths.push_back(std::string(appData) + "\\mpv\\mpv.exe");
                    if (localAppData) {
                        searchPaths.push_back(std::string(localAppData) + "\\mpv\\mpv.exe");
                        searchPaths.push_back(std::string(localAppData) + "\\Programs\\mpv\\mpv.exe");
                    }
                    if (userProfile) {
                        searchPaths.push_back(std::string(userProfile) + "\\mpv\\mpv.exe");
                        searchPaths.push_back(std::string(userProfile) + "\\Downloads\\mpv\\mpv.exe");
                    }
                    searchPaths.push_back("C:\\mpv\\mpv.exe");
                    searchPaths.push_back("D:\\mpv\\mpv.exe");
                    searchPaths.push_back(".\\mpv.exe");
                    searchPaths.push_back(".\\portable\\mpv.exe");
                    searchPaths.push_back(".\\bin\\mpv.exe");
                    searchPaths.push_back(".\\tools\\mpv.exe");
                    
                    for (const auto& p : searchPaths) {
                        WIN32_FIND_DATAA fd;
                        if (FindFirstFileA(p.c_str(), &fd) != INVALID_HANDLE_VALUE) {
                            mpvPath = p;
                            break;
                        }
                    }
                }
            }
            
            if (!mpvPath.empty()) {
                size_t pos = mpvPath.rfind(".com");
                if (pos != std::string::npos && pos == mpvPath.length() - 4) {
                    mpvPath = mpvPath.substr(0, pos) + ".exe";
                }
            }
            
            if (mpvPath.empty()) {
                std::cerr << "Error: mpv not found! Please install mpv." << std::endl;
                return;
            }
            
            std::string cmdLine = "\"" + mpvPath + "\"";
            for (auto& a : args) {
                cmdLine += " \"" + a + "\"";
            }
            
            STARTUPINFOA si = {};
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
            si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
            si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
            
            PROCESS_INFORMATION pi = {};
            if (!CreateProcessA(mpvPath.c_str(), const_cast<LPSTR>(cmdLine.c_str()), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
                DWORD error = GetLastError();
                LPSTR errorMsg = nullptr;
                FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error, 0, (LPSTR)&errorMsg, 0, NULL);
                std::cerr << "Failed to start mpv: " << (errorMsg ? errorMsg : "unknown error") << std::endl;
                if (errorMsg) LocalFree(errorMsg);
                return;
            }
            childPid_ = pi.dwProcessId;
            CloseHandle(pi.hThread);
#else
            std::vector<char*> argv;
            argv.push_back(const_cast<char*>("mpv"));
            for (auto& a : args) {
                argv.push_back(const_cast<char*>(a.c_str()));
            }
            argv.push_back(nullptr);
            
#ifndef _WIN32
            setenv("PIPEWIRE_QUIET", "1", 1);
            setenv("PIPEWIRE_DEBUG", "0", 1);
#endif
            
            childPid_ = fork();
            if (childPid_ == 0) {
                execvp("mpv", argv.data());
                _exit(1);
            } else if (childPid_ < 0) {
                std::cerr << "Failed to fork mpv" << std::endl;
                return;
            }
#endif
            
            isPlaying_ = true;
            isPaused_ = false;
            currentFile_ = current;
            
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            sendMpvCommand("{\"command\":[\"set_property\",\"fullscreen\",true]}");
        }
    }
}

void Player::SetDisplay(const std::string& display) {
    std::lock_guard<std::recursive_mutex> lock(mu_);
    displayName_ = display;
    
    if (isPlaying_ && !currentFile_.empty()) {
        std::string current = currentFile_;
        
#ifdef _WIN32
        if (!socketPath_.empty()) {
            HANDLE h = CreateFileA(socketPath_.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
            if (h != INVALID_HANDLE_VALUE) {
                std::string cmd = "{\"command\":[\"quit\"]}\n";
                DWORD written = 0;
                WriteFile(h, cmd.c_str(), cmd.size(), &written, NULL);
                CloseHandle(h);
            }
        }
#else
        if (!socketPath_.empty()) {
            int fd = connectToMpv();
            if (fd >= 0) {
                std::string cmd = "{\"command\":[\"quit\"]}\n";
                write(fd, cmd.c_str(), cmd.size());
                close(fd);
            }
        }
#endif
        
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        stopInternal();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        isPlaying_ = false;
        isPaused_ = false;
        
        Play(current);
    }
}

void Player::ShowText(const std::string& text, int fontSize, bool show, int position) {
    std::lock_guard<std::recursive_mutex> lock(mu_);
    if (socketPath_.empty()) return;
    
    std::string alignX = "center";
    std::string alignY = "center";
    
    if (position == 1) {
        alignX = "left";
        alignY = "top";
    } else if (position == 2) {
        alignX = "right";
        alignY = "top";
    } else if (position == 3) {
        alignX = "left";
        alignY = "bottom";
    } else if (position == 4) {
        alignX = "right";
        alignY = "bottom";
    }
    
    if (show && !text.empty()) {
        tctEnabled_ = true;
        tctText_ = text;
        tctFontSize_ = fontSize;
        tctPosition_ = position;
        
        sendMpvCommand("{\"command\":[\"set_property\",\"osd-level\",3]}");
        sendMpvCommand("{\"command\":[\"set_property\",\"osd-font-size\"," + std::to_string(fontSize) + "]}");
        sendMpvCommand("{\"command\":[\"set_property\",\"osd-align-x\",\"" + alignX + "\"]}");
        sendMpvCommand("{\"command\":[\"set_property\",\"osd-align-y\",\"" + alignY + "\"]}");
        sendMpvCommand("{\"command\":[\"set_property\",\"osd-duration\",999999]}");
        std::string cmd = "{\"command\":[\"show-text\",\"" + text + "\",999999]}";
        sendMpvCommand(cmd);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        sendMpvCommand("{\"command\":[\"set_property\",\"osd-level\",0]}");
    } else {
        tctEnabled_ = false;
        sendMpvCommand("{\"command\":[\"show-text\",\"\",0]}");
        sendMpvCommand("{\"command\":[\"set_property\",\"osd-level\",0]}");
    }
}

void Player::updateFPSDisplay() {
}

std::string Player::StopAtFrame(int frameNum) {
    std::lock_guard<std::recursive_mutex> lock(mu_);
    if (!isPlaying_ || socketPath_.empty()) return "";
    
    double totalFrames = 0;
    double fps = 30;
    
    int fd = connectToMpv();
    if (fd >= 0) {
        std::string cmd = "{\"command\":[\"get_property\",\"estimated-frame-count\"]}\n";
        write(fd, cmd.c_str(), cmd.size());
        char buf[1024] = {};
        read(fd, buf, sizeof(buf));
        const char* p = strstr(buf, "\"data\":");
        if (p) sscanf(p + 7, "%lf", &totalFrames);
        close(fd);
    }
    
    fd = connectToMpv();
    if (fd >= 0) {
        std::string cmd = "{\"command\":[\"get_property\",\"estimated-vf-fps\"]}\n";
        write(fd, cmd.c_str(), cmd.size());
        char buf[1024] = {};
        read(fd, buf, sizeof(buf));
        const char* p = strstr(buf, "\"data\":");
        if (p) sscanf(p + 7, "%lf", &fps);
        close(fd);
    }
    
    if (totalFrames <= 0 || fps <= 0) {
        if (frameNum == 0) {
            sendMpvCommand("{\"command\":[\"seek\",0.0,\"absolute-percent\"]}");
        } else {
            sendMpvCommand("{\"command\":[\"seek\",99.0,\"absolute-percent\"]}");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        sendMpvCommand("{\"command\":[\"set_property\",\"pause\",true]}");
        isPaused_ = true;
        return std::to_string(frameNum);
    }
    
    double targetFrame = frameNum;
    double targetTime = targetFrame / fps;
    
    sendMpvCommand("{\"command\":[\"seek\"," + std::to_string(targetTime) + ",\"absolute\"]}");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    sendMpvCommand("{\"command\":[\"set_property\",\"pause\",true]}");
    isPaused_ = true;
    
    return std::to_string(frameNum);
}

std::string Player::StopAtSeconds(double seconds) {
    std::lock_guard<std::recursive_mutex> lock(mu_);
    if (!isPlaying_ || socketPath_.empty()) return "";
    
    sendMpvCommand("{\"command\":[\"seek\"," + std::to_string(seconds) + ",\"absolute\"]}");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    sendMpvCommand("{\"command\":[\"set_property\",\"pause\",true]}");
    isPaused_ = true;
    
    return std::to_string(seconds);
}
