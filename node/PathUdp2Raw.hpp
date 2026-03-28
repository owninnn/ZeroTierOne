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
 * Udp2Raw extension for Path class
 * Manages udp2raw transport for a path
 */

#ifndef ZT_PATH_UDP2RAW_HPP
#define ZT_PATH_UDP2RAW_HPP

#include "InetAddress.hpp"
#include "Udp2RawProtocol.hpp"
#include "../osdep/Udp2RawSocket.hpp"

#include <atomic>
#include <memory>

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Udp2Raw path extension
 * 
 * This class extends a Path with udp2raw capabilities.
 * It manages the udp2raw socket and handles the upgrade/downgrade
 * between standard UDP and udp2raw transport.
 */
class PathUdp2Raw {
public:
    // Path transport type
    enum TransportType {
        TRANSPORT_UDP = 0,      // Standard UDP
        TRANSPORT_UDP2RAW_TCP,  // Fake TCP encapsulation
        TRANSPORT_UDP2RAW_ICMP  // ICMP encapsulation
    };
    
    // Udp2Raw state
    enum Udp2RawState {
        UDP2RAW_DISABLED = 0,   // Not using udp2raw
        UDP2RAW_PROBING,        // Probing peer capability
        UDP2RAW_HANDSHAKE,      // Handshake in progress
        UDP2RAW_ESTABLISHED,    // Udp2raw connection established
        UDP2RAW_FAILED          // Failed to establish udp2raw
    };
    
    // Configuration
    struct Config {
        bool enabled;                    // Enable udp2raw for this path
        Udp2RawProtocol::TransportMode preferredMode;
        uint16_t localPort;
        uint16_t remotePort;
        uint16_t icmpId;
        uint32_t probeIntervalMs;
        uint32_t handshakeTimeoutMs;
        uint32_t fallbackDelayMs;        // Delay before falling back to UDP
        bool autoUpgrade;                // Auto upgrade to udp2raw
        bool autoDowngrade;              // Auto downgrade on failure
        
        Config() :
            enabled(true),
            preferredMode(Udp2RawProtocol::MODE_FAKE_TCP),
            localPort(0),
            remotePort(443),
            icmpId(0x1234),
            probeIntervalMs(30000),
            handshakeTimeoutMs(10000),
            fallbackDelayMs(60000),
            autoUpgrade(true),
            autoDowngrade(true)
        {}
    };
    
    /**
     * Constructor
     */
    PathUdp2Raw();
    
    /**
     * Destructor
     */
    ~PathUdp2Raw();
    
    /**
     * Initialize udp2raw for this path
     * @param localAddr Local address
     * @param remoteAddr Remote address
     * @param config Configuration
     * @return true on success
     */
    bool init(const InetAddress& localAddr, const InetAddress& remoteAddr, const Config& config);
    
    /**
     * Shutdown udp2raw
     */
    void shutdown();
    
    /**
     * Get current transport type
     */
    TransportType getTransportType() const { return _transportType.load(); }
    
    /**
     * Get current udp2raw state
     */
    Udp2RawState getState() const { return _state.load(); }
    
    /**
     * Check if udp2raw is active (established)
     */
    bool isActive() const { return _state.load() == UDP2RAW_ESTABLISHED; }
    
    /**
     * Check if we should use udp2raw for sending
     */
    bool shouldUseUdp2Raw() const;
    
    /**
     * Send data via udp2raw
     * @param data Data to send
     * @param len Data length
     * @return true on success
     */
    bool send(const void* data, unsigned int len);
    
    /**
     * Receive data from udp2raw
     * @param buf Buffer for received data
     * @param bufLen Buffer size
     * @param timeoutMs Timeout in milliseconds
     * @return Number of bytes received, 0 if no data, -1 on error
     */
    int receive(void* buf, unsigned int bufLen, int timeoutMs = 0);
    
    /**
     * Process incoming packet (may be udp2raw control packet)
     * @param data Packet data
     * @param len Packet length
     * @return true if packet was consumed (control packet)
     */
    bool processIncomingPacket(const void* data, unsigned int len);
    
    /**
     * Start probing peer for udp2raw capability
     */
    void startProbe();
    
    /**
     * Start handshake to establish udp2raw connection
     */
    void startHandshake();
    
    /**
     * Handle probe response from peer
     */
    void handleProbeResponse();
    
    /**
     * Handle handshake response
     * @param mode Agreed transport mode
     * @param port Agreed port
     */
    void handleHandshakeAck(Udp2RawProtocol::TransportMode mode, uint16_t port);
    
    /**
     * Mark udp2raw as failed, fall back to UDP
     */
    void markFailed();
    
    /**
     * Get last activity time
     */
    int64_t getLastActivity() const { return _lastActivity.load(); }
    
    /**
     * Update activity timestamp
     */
    void updateActivity(int64_t now);
    
    /**
     * Check if we should retry udp2raw
     * @param now Current time
     * @return true if should retry
     */
    bool shouldRetry(int64_t now) const;
    
    /**
     * Get statistics
     */
    void getStats(uint64_t& sent, uint64_t& received, uint64_t& errors) const;
    
    /**
     * Check if udp2raw is available on this system
     */
    static bool isSystemSupported();
    
    /**
     * Get transport type name
     */
    static const char* getTransportTypeName(TransportType type);
    
    /**
     * Get state name
     */
    static const char* getStateName(Udp2RawState state);

private:
    std::unique_ptr<Udp2RawSocket> _socket;
    std::atomic<TransportType> _transportType;
    std::atomic<Udp2RawState> _state;
    std::atomic<int64_t> _lastActivity;
    std::atomic<int64_t> _lastProbeTime;
    std::atomic<int64_t> _lastFailureTime;
    std::atomic<uint64_t> _packetsSent;
    std::atomic<uint64_t> _packetsReceived;
    std::atomic<uint64_t> _errors;
    
    InetAddress _localAddr;
    InetAddress _remoteAddr;
    Config _config;
    
    bool _probeSent;
    bool _handshakeSent;
    int64_t _handshakeStartTime;
};

} // namespace ZeroTier

#endif // ZT_PATH_UDP2RAW_HPP
