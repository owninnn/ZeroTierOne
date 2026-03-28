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
 *
 * --
 *
 * Udp2Raw global configuration
 */

#ifndef ZT_UDP2RAW_CONFIG_HPP
#define ZT_UDP2RAW_CONFIG_HPP

#include "Udp2RawProtocol.hpp"

#include <atomic>
#include <cstdint>
#include <string>

namespace ZeroTier {

/**
 * Global udp2raw configuration
 * 
 * This class manages global udp2raw settings from local.conf
 * Default: enabled with auto mode
 */
class Udp2RawConfig {
public:
    // Global enable/disable
    enum GlobalMode {
        MODE_DISABLED = 0,  // Completely disabled
        MODE_AUTO,          // Auto-detect and upgrade (default)
        MODE_PREFERRED,     // Prefer udp2raw over UDP
        MODE_REQUIRED       // Require udp2raw (fail without it)
    };
    
    // Configuration structure
    struct Settings {
        GlobalMode mode;                    // Global mode
        Udp2RawProtocol::TransportMode transportMode;  // Preferred transport
        uint16_t localPort;                 // Local port (0 = auto)
        uint16_t remotePort;                // Remote port for FakeTCP
        uint16_t icmpId;                    // ICMP identifier
        uint32_t probeIntervalMs;           // Probe interval
        uint32_t handshakeTimeoutMs;        // Handshake timeout
        uint32_t fallbackDelayMs;           // Fallback delay after failure
        uint32_t upgradeDelayMs;            // Delay before upgrading to udp2raw
        bool autoUpgrade;                   // Auto upgrade paths
        bool autoDowngrade;                 // Auto downgrade on failure
        bool logVerbose;                    // Verbose logging
        std::string fakeTcpPorts;           // Comma-separated list of ports to try
        
        Settings() :
            mode(MODE_AUTO),
            transportMode(Udp2RawProtocol::MODE_FAKE_TCP),
            localPort(0),
            remotePort(443),
            icmpId(0x1234),
            probeIntervalMs(30000),
            handshakeTimeoutMs(10000),
            fallbackDelayMs(60000),
            upgradeDelayMs(5000),
            autoUpgrade(true),
            autoDowngrade(true),
            logVerbose(false),
            fakeTcpPorts("443,80,8080,8443,53")
        {}
    };
    
    /**
     * Get singleton instance
     */
    static Udp2RawConfig& getInstance();
    
    /**
     * Load configuration from JSON
     * @param jsonConfig JSON object from local.conf (nlohmann::json)
     * @return true if valid configuration loaded
     */
    bool loadFromJson(const void* jsonConfig);
    
    /**
     * Load configuration from JSON string
     * @param jsonStr JSON string
     * @return true if valid configuration loaded
     */
    bool loadFromJsonString(const char* jsonStr);
    
    /**
     * Get current settings
     */
    const Settings& getSettings() const { return _settings; }
    
    /**
     * Check if udp2raw is enabled
     */
    bool isEnabled() const { return _settings.mode != MODE_DISABLED; }
    
    /**
     * Check if udp2raw is required
     */
    bool isRequired() const { return _settings.mode == MODE_REQUIRED; }
    
    /**
     * Check if auto-upgrade is enabled
     */
    bool autoUpgrade() const { return _settings.autoUpgrade && isEnabled(); }
    
    /**
     * Check if auto-downgrade is enabled
     */
    bool autoDowngrade() const { return _settings.autoDowngrade; }
    
    /**
     * Get mode name
     */
    static const char* getModeName(GlobalMode mode);
    
    /**
     * Parse mode from string
     */
    static GlobalMode parseMode(const char* str);
    
    /**
     * Parse transport mode from string
     */
    static Udp2RawProtocol::TransportMode parseTransportMode(const char* str);
    
    /**
     * Get default local.conf snippet
     */
    static const char* getDefaultConfigSnippet();

private:
    Udp2RawConfig() = default;
    ~Udp2RawConfig() = default;
    Udp2RawConfig(const Udp2RawConfig&) = delete;
    Udp2RawConfig& operator=(const Udp2RawConfig&) = delete;
    
    Settings _settings;
};

} // namespace ZeroTier

#endif // ZT_UDP2RAW_CONFIG_HPP
