#ifndef AUDIO_DEVICE_H
#define AUDIO_DEVICE_H

#include <string>
#include <vector>

struct AudioDevice {
    std::string displayName;
    std::string deviceID;
};

std::vector<AudioDevice> getAudioDevices();
std::vector<std::string> getAudioDevicesWithIndex();
std::string getAudioDeviceIDByIndex(int index);
std::string getAudioDeviceIDByName(const std::string& name);

#endif
