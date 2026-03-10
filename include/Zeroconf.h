#ifndef ZEROCONF_H
#define ZEROCONF_H

#include <string>
#include <memory>

class Zeroconf {
public:
    static Zeroconf& getInstance();
    void start(const std::string& serviceName, const std::string& serviceType, int port);
    void stop();
    bool isRunning() const { return m_running; }
    
private:
    Zeroconf();
    ~Zeroconf();
    Zeroconf(const Zeroconf&) = delete;
    Zeroconf& operator=(const Zeroconf&) = delete;
    
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    bool m_running = false;
};

#endif // ZEROCONF_H
