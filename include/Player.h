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

#ifndef PLAYER_H
#define PLAYER_H

#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <vector>
#include <thread>
#include <chrono>

struct mpv_handle;
struct mpv_event;

class Player {
public:
    Player();
    ~Player();

    void Play(const std::string& filename);
    void Stop();
    void Pause();
    bool IsPaused() const;
    void SetVolume(int vol);
    void SetLoop(bool enable);
    void SetFullscreen(int mode);
    
    bool IsPlaying() const { return isPlaying_; }
    std::string GetCurrentFile() const;
    std::string getHelloVideoName() const;
    
    double GetPosition();
    double GetTime();
    double GetFPS();
    double GetDuration();
    std::string GetFrameTiming();
    void SetFPSDisplay(int mode);
    void updateFPSDisplay();
    
    void SeekToPercent(double percent);
    void SeekToFrame(int frame);
    void SeekToTime(double seconds);
    
    void SetAudioDevice(const std::string& device);
    void SetDisplay(const std::string& display);
    void ShowText(const std::string& text, int fontSize, bool show, int position = 0);
    
    void SetStopMode(int mode) { stopMode_ = mode; }
    int GetChildPid() const { return childPid_; }
    std::string StopAtFrame(int frameNum);
    std::string StopAtSeconds(double seconds);
    void StopExit();
    bool IsLoopEnabled() const { return loopEnabled_; }
    
    bool seekingEnabled_ = true;
    mutable std::recursive_mutex mu_;

private:
    std::string getSocketPath();
    void stopInternal();
    void cleanupOrphanMpvs();
    void sendMpvCommand(const std::string& cmd);
    int connectToMpv();
    
    std::string findVideo(const std::string& filename) const;
    std::string findHelloVideo() const;
    
    bool isPlaying_ = false;
    bool isPaused_ = false;
    bool loopEnabled_ = false;
    bool loopSetByUser_ = false;
    bool isScreensaver_ = false;
    bool isUserStopped_ = false;
    int fullscreenMode_ = 3;
    int stopMode_ = -1;
    int volume_ = 100;
    int fpsDisplayMode_ = 0;
    bool tctEnabled_ = false;
    std::string tctText_;
    int tctFontSize_ = 48;
    int tctPosition_ = 0;
    std::atomic<bool> fpsDisplayRunning_{false};
    std::thread fpsDisplayThread_;
    std::string currentFile_;
    std::string audioDevice_;
    std::string displayName_;
    
    std::string socketPath_;
    
    int childPid_ = -1;
    
    std::string exeDir_;
};

std::string getVideoDir();
std::vector<std::string> getVideoSearchPaths();
std::vector<std::string> getAllVideos();

#endif
