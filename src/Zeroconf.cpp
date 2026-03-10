#include "../include/Zeroconf.h"
#include "../include/Platform.h"
#include <iostream>
#include <cstring>
#include <memory>

#ifdef __APPLE__
    #include <CoreFoundation/CoreFoundation.h>
    #include <dns_sd.h>
    #include <arpa/inet.h>
#elif _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #include <dns_sd.h>
#else
    #include <avahi-client/client.h>
    #include <avahi-client/service.h>
    #include <avahi-glib/glib-watch.h>
    #include <glib.h>
#endif

struct Zeroconf::Impl {
#ifdef __APPLE__
    DNSServiceRef sdRef = nullptr;
#elif _WIN32
    DNSServiceRef sdRef = nullptr;
#else
    AvahiThreadedPoll* poll = nullptr;
    AvahiClient* client = nullptr;
    AvahiEntryGroup* group = nullptr;
    std::string serviceName;
    std::string serviceType;
    int port = 0;
    
    static void callback(AvahiEntryGroup* g, AvahiClient* c, AvahiEntryGroupState state, void* userdata) {
        auto* self = static_cast<Impl*>(userdata);
        if (state == AVAHI_ENTRY_GROUP_ESTABLISHED) {
            std::cout << "Avahi service '" << self->serviceName << "' registered successfully" << std::endl;
        } else if (state == AVAHI_ENTRY_GROUP_COLLISION) {
            std::cerr << "Avahi service name collision!" << std::endl;
        }
    }
#endif
};

Zeroconf::Zeroconf() : m_impl(std::make_unique<Impl>()) {}
Zeroconf::~Zeroconf() { stop(); }

Zeroconf& Zeroconf::getInstance() {
    static Zeroconf instance;
    return instance;
}

void Zeroconf::start(const std::string& serviceName, const std::string& serviceType, int port) {
    if (m_running) {
        stop();
    }
    
#ifdef __APPLE__
    std::cout << "Starting Bonjour service: " << serviceName << " type: " << serviceType << " port: " << port << std::endl;
    
    DNSServiceErrorType err = DNSServiceRegister(
        &m_impl->sdRef,
        0,
        0,
        serviceName.c_str(),
        serviceType.c_str(),
        nullptr,
        nullptr,
        htons(port),
        0,
        nullptr,
        nullptr,
        nullptr
    );
    
    if (err == kDNSServiceErr_NoError) {
        m_running = true;
        std::cout << "Bonjour service registered successfully" << std::endl;
    } else {
        std::cerr << "Bonjour registration failed with error: " << err << std::endl;
    }
    
#elif _WIN32
    std::cout << "Starting Bonjour service: " << serviceName << " type: " << serviceType << " port: " << port << std::endl;
    
    DNSServiceErrorType err = DNSServiceRegister(
        &m_impl->sdRef,
        0,
        0,
        serviceName.c_str(),
        serviceType.c_str(),
        nullptr,
        nullptr,
        port,
        0,
        nullptr,
        nullptr,
        nullptr
    );
    
    if (err == kDNSServiceErr_NoError) {
        m_running = true;
        std::cout << "Bonjour service registered successfully" << std::endl;
    } else {
        std::cerr << "Bonjour registration failed with error: " << err << std::endl;
    }
    
#else
    std::cout << "Starting Avahi service: " << serviceName << " type: " << serviceType << " port: " << port << std::endl;
    
    m_impl->poll = avahi_threaded_poll_new();
    
    if (!m_impl->poll) {
        std::cerr << "Failed to create Avahi poll" << std::endl;
        return;
    }
    
    m_impl->client = avahi_client_new(avahi_threaded_poll_get(m_impl->poll), AVAHI_CLIENT_NO_FAIL, nullptr, nullptr, nullptr);
    
    if (!m_impl->client) {
        std::cerr << "Failed to create Avahi client" << std::endl;
        avahi_threaded_poll_free(m_impl->poll);
        m_impl->poll = nullptr;
        return;
    }
    
    m_impl->serviceName = serviceName;
    m_impl->serviceType = serviceType;
    m_impl->port = port;
    
    m_impl->group = avahi_entry_group_new(m_impl->client, Impl::callback, m_impl.get());
    
    if (!m_impl->group) {
        std::cerr << "Failed to create Avahi entry group" << std::endl;
        avahi_client_free(m_impl->client);
        avahi_threaded_poll_free(m_impl->poll);
        return;
    }
    
    avahi_entry_group_add_service(
        m_impl->group,
        AVAHI_IF_UNSPEC,
        AVAHI_PROTO_UNSPEC,
        0,
        serviceName.c_str(),
        serviceType.c_str(),
        nullptr,
        nullptr,
        port,
        nullptr
    );
    
    avahi_entry_group_commit(m_impl->group);
    avahi_threaded_poll_start(m_impl->poll);
    
    m_running = true;
    std::cout << "Avahi service registered successfully" << std::endl;
#endif
}

void Zeroconf::stop() {
    if (!m_running) {
        return;
    }
    
#ifdef __APPLE__
    if (m_impl->sdRef) {
        DNSServiceRefDeallocate(m_impl->sdRef);
        m_impl->sdRef = nullptr;
    }
#elif _WIN32
    if (m_impl->sdRef) {
        DNSServiceRefDeallocate(m_impl->sdRef);
        m_impl->sdRef = nullptr;
    }
#else
    if (m_impl->group) {
        avahi_entry_group_free(m_impl->group);
        m_impl->group = nullptr;
    }
    if (m_impl->client) {
        avahi_client_free(m_impl->client);
        m_impl->client = nullptr;
    }
    if (m_impl->poll) {
        avahi_threaded_poll_stop(m_impl->poll);
        avahi_threaded_poll_free(m_impl->poll);
        m_impl->poll = nullptr;
    }
#endif
    
    m_running = false;
    std::cout << "Zeroconf service stopped" << std::endl;
}
