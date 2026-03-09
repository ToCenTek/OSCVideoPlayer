#ifndef OSC_MESSAGE_H
#define OSC_MESSAGE_H

#include <string>
#include <vector>
#include <cstdint>

enum class OSCType { NONE, INT32, FLOAT, STRING, BOOL };

struct OSCArg {
    OSCType type = OSCType::NONE;
    int32_t intVal = 0;
    float floatVal = 0.0f;
    bool boolVal = false;
    std::string stringVal;
};

class OSCMessage {
public:
    OSCMessage() = default;
    explicit OSCMessage(const std::string& address) : address_(address) {}

    void setAddress(const std::string& addr) { address_ = addr; }
    const std::string& getAddress() const { return address_; }

    void addInt(int32_t v) { OSCArg a; a.type = OSCType::INT32; a.intVal = v; args_.push_back(a); }
    void addFloat(float v) { OSCArg a; a.type = OSCType::FLOAT; a.floatVal = v; args_.push_back(a); }
    void addString(const std::string& v) { OSCArg a; a.type = OSCType::STRING; a.stringVal = v; args_.push_back(a); }
    void addBool(bool v) { OSCArg a; a.type = OSCType::BOOL; a.boolVal = v; args_.push_back(a); }

    std::vector<OSCArg>& getArgs() { return args_; }
    const std::vector<OSCArg>& getArgs() const { return args_; }
    size_t argCount() const { return args_.size(); }
    OSCType getArgType(size_t idx) const {
        if (idx >= args_.size()) return OSCType::NONE;
        return args_[idx].type;
    }

    int32_t getIntArg(size_t idx, int32_t def = 0) const {
        if (idx >= args_.size() || args_[idx].type != OSCType::INT32) return def;
        return args_[idx].intVal;
    }
    float getFloatArg(size_t idx, float def = 0.0f) const {
        if (idx >= args_.size()) return def;
        if (args_[idx].type == OSCType::FLOAT) return args_[idx].floatVal;
        if (args_[idx].type == OSCType::INT32) return (float)args_[idx].intVal;
        if (args_[idx].type == OSCType::STRING) {
            return std::stof(args_[idx].stringVal);
        }
        return def;
    }
    const std::string& getStringArg(size_t idx) const {
        static std::string empty;
        if (idx >= args_.size() || args_[idx].type != OSCType::STRING) return empty;
        return args_[idx].stringVal;
    }

private:
    std::string address_;
    std::vector<OSCArg> args_;
};

class OSCBundle {
public:
    OSCBundle() = default;
    void addMessage(const OSCMessage& msg) { messages_.push_back(msg); }
    const std::vector<OSCMessage>& getMessages() const { return messages_; }

private:
    std::vector<OSCMessage> messages_;
};

OSCMessage parseOSCPacket(const char* data, size_t len);

#endif
