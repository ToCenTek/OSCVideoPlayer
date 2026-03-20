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
