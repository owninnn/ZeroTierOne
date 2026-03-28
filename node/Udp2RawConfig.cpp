/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Udp2RawConfig.hpp"

#include <cstring>
#include <cstdio>

// For JSON parsing - include nlohmann/json
// Note: In actual build, this would use the same JSON library as the rest of ZeroTier
// For now we assume a simple interface

namespace ZeroTier {

Udp2RawConfig& Udp2RawConfig::getInstance()
{
    static Udp2RawConfig instance;
    return instance;
}

const char* Udp2RawConfig::getModeName(GlobalMode mode)
{
    switch (mode) {
    case MODE_DISABLED:
        return "disabled";
    case MODE_AUTO:
        return "auto";
    case MODE_PREFERRED:
        return "preferred";
    case MODE_REQUIRED:
        return "required";
    default:
        return "unknown";
    }
}

Udp2RawConfig::GlobalMode Udp2RawConfig::parseMode(const char* str)
{
    if (!str || strlen(str) == 0) {
        return MODE_AUTO; // Default
    }
    
    if (strcmp(str, "disabled") == 0 || strcmp(str, "off") == 0) {
        return MODE_DISABLED;
    }
    if (strcmp(str, "auto") == 0) {
        return MODE_AUTO;
    }
    if (strcmp(str, "preferred") == 0) {
        return MODE_PREFERRED;
    }
    if (strcmp(str, "required") == 0 || strcmp(str, "force") == 0) {
        return MODE_REQUIRED;
    }
    
    return MODE_AUTO; // Default
}

Udp2RawProtocol::TransportMode Udp2RawConfig::parseTransportMode(const char* str)
{
    if (!str || strlen(str) == 0) {
        return Udp2RawProtocol::MODE_FAKE_TCP; // Default
    }
    
    if (strcmp(str, "tcp") == 0 || strcmp(str, "faketcp") == 0) {
        return Udp2RawProtocol::MODE_FAKE_TCP;
    }
    if (strcmp(str, "icmp") == 0) {
        return Udp2RawProtocol::MODE_ICMP;
    }
    if (strcmp(str, "auto") == 0) {
        return Udp2RawProtocol::MODE_FAKE_TCP; // Default to TCP for auto
    }
    
    return Udp2RawProtocol::MODE_FAKE_TCP; // Default
}

bool Udp2RawConfig::loadFromJson(const void* jsonConfig)
{
    // In actual implementation, this would parse the JSON
    // For now, just use defaults
    
    // Example parsing (pseudo-code):
    // const json& config = *(const json*)jsonConfig;
    // if (config.contains("udp2raw")) {
    //     const json& u2r = config["udp2raw"];
    //     _settings.mode = parseMode(u2r.value("mode", "auto").c_str());
    //     _settings.transportMode = parseTransportMode(u2r.value("transport", "tcp").c_str());
    //     _settings.localPort = u2r.value("localPort", 0);
    //     _settings.remotePort = u2r.value("remotePort", 443);
    //     ...
    // }
    
    // For now, just return true with defaults
    return true;
}

const char* Udp2RawConfig::getDefaultConfigSnippet()
{
    return R"({
    "settings": {
        "udp2raw": {
            "mode": "auto",
            "transport": "tcp",
            "localPort": 0,
            "remotePort": 443,
            "icmpId": 4660,
            "probeInterval": 30000,
            "handshakeTimeout": 10000,
            "fallbackDelay": 60000,
            "upgradeDelay": 5000,
            "autoUpgrade": true,
            "autoDowngrade": true,
            "logVerbose": false,
            "fakeTcpPorts": "443,80,8080,8443,53,22"
        }
    }
})";
}

} // namespace ZeroTier
