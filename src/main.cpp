#include "../include/Player.h"
#include "../include/OSCServer.h"
#include "../include/Platform.h"
#include <iostream>
#include <csignal>
#include <thread>
#include <ctime>
#include <iomanip>
#include <sstream>

#ifndef _WIN32
#include <unistd.h>
#include <cstdlib>
#else
#include <windows.h>
#endif

std::unique_ptr<Player> g_player;
std::unique_ptr<OSCServer> g_server;
std::atomic<bool> g_running{true};

void signalHandler(int sig) {
    std::cout << "\nShutting down..." << std::endl;
    g_running = false;
    
    if (g_player) {
        int pid = g_player->GetChildPid();
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
    }
    
    if (g_server) g_server->stop();
    _exit(0);
}

static const char* helpText = R"(
Available OSC Commands:
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
/tct[text/size/pos] - Show text on screen (pos: 0=center, 1=top-left, 2=top-right, 3=bottom-left, 4=bottom-right)
/port[name/port] - Set reply port (e.g., chataigne/12000)
/list/audio      - List audio devices
/list/display   - List displays
/list/videos    - List all videos
/reboot          - Reboot system
/shutdown        - Shutdown system
/help            - Show this help
)";

int main(int argc, char* argv[]) {
#ifndef _WIN32
    setenv("PIPEWIRE_QUIET", "1", 1);
    setenv("PIPEWIRE_DEBUG", "0", 1);
    setenv("WARNINGS", "0", 1);
#endif

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0') << std::setw(3) << ms.count();
    std::cout << "Starting at: " << oss.str() << std::endl;
    
    std::cout << "OSCPlayer v1.0.0 - OSC-controlled video player" << std::endl;
    std::cout << "=================================================" << std::endl;
    std::cout << helpText << std::endl;
    
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    g_player = std::make_unique<Player>();
    
    std::cout << "Cleaning up orphan mpv processes..." << std::endl;
#ifdef _WIN32
    system("taskkill /F /IM mpv.exe 2>nul");
#else
    system("pkill -9 mpv 2>/dev/null");
#endif
    
    g_server = std::make_unique<OSCServer>("0.0.0.0", 8000, g_player.get());
    
    if (!g_server->start()) {
        std::cerr << "Failed to start OSC server" << std::endl;
        return 1;
    }
    
    std::cout << "\nSearching for hello video..." << std::endl;
    std::string helloVideo = g_player->getHelloVideoName();
    std::cout << "Starting default playback: " << helloVideo << std::endl;
    g_player->Play("");
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (!g_player->IsPlaying()) {
        std::cout << "No hello video found, checking for any available video..." << std::endl;
        auto allVideos = getAllVideos();
        if (!allVideos.empty()) {
            std::string filename = allVideos[0];
            size_t pos = filename.find_last_of('/');
            if (pos != std::string::npos) {
                filename = filename.substr(pos + 1);
            }
            std::cout << "Playing first available video: " << filename << std::endl;
            g_player->Play(filename);
        } else {
            std::cout << "No videos found. Waiting for user to add videos..." << std::endl;
        }
    }
    
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}
