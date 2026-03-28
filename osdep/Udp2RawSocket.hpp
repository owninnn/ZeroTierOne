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
 * Raw socket wrapper for udp2raw protocol
 * Handles FakeTCP/ICMP packet sending and receiving
 */

#ifndef ZT_UDP2RAW_SOCKET_HPP
#define ZT_UDP2RAW_SOCKET_HPP

#include "../node/InetAddress.hpp"
#include "../node/Udp2RawProtocol.hpp"

#include <atomic>
#include <cstdint>
#include <string>

// Platform-specific includes
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace ZeroTier {

/**
 * Raw socket handler for udp2raw protocol
 * 
 * This class manages raw sockets for sending/receiving FakeTCP and ICMP packets.
 * Requires root/admin privileges on most systems.
 */
class Udp2RawSocket {
public:
    // Socket states
    enum State {
        STATE_CLOSED = 0,
        STATE_OPENING,
        STATE_OPEN,
        STATE_ERROR
    };
    
    // Connection states for FakeTCP
    enum TcpState {
        TCP_CLOSED = 0,
        TCP_SYN_SENT,
        TCP_ESTABLISHED,
        TCP_FIN_WAIT
    };
    
    // Configuration
    struct Config {
        Udp2RawProtocol::TransportMode mode;  // FakeTCP or ICMP
        uint16_t localPort;                    // Local port (for FakeTCP)
        uint16_t remotePort;                   // Remote port (for FakeTCP)
        uint16_t icmpId;                       // ICMP identifier
        uint32_t tcpSeq;                       // Initial TCP sequence number
        uint32_t tcpAck;                       // Initial TCP ack number
        bool autoHandshake;                    // Auto send handshake
        bool autoKeepalive;                    // Auto send keepalive
        uint32_t keepaliveInterval;            // Keepalive interval in ms
        
        Config() :
            mode(Udp2RawProtocol::MODE_FAKE_TCP),
            localPort(0),
            remotePort(443),
            icmpId(0x1234),
            tcpSeq(0),
            tcpAck(0),
            autoHandshake(true),
            autoKeepalive(true),
            keepaliveInterval(30000)
        {}
    };
    
    // Statistics
    struct Stats {
        uint64_t packetsSent;
        uint64_t packetsReceived;
        uint64_t bytesSent;
        uint64_t bytesReceived;
        uint64_t errors;
        uint64_t checksumErrors;
        
        Stats() :
            packetsSent(0),
            packetsReceived(0),
            bytesSent(0),
            bytesReceived(0),
            errors(0),
            checksumErrors(0)
        {}
    };
    
    /**
     * Constructor
     */
    Udp2RawSocket();
    
    /**
     * Destructor - closes socket
     */
    ~Udp2RawSocket();
    
    /**
     * Open raw socket
     * @param localAddr Local address to bind (can be wildcard)
     * @param config Configuration
     * @return true on success
     */
    bool open(const InetAddress& localAddr, const Config& config);
    
    /**
     * Close socket
     */
    void close();
    
    /**
     * Check if socket is open and ready
     */
    bool isOpen() const { return _state == STATE_OPEN; }
    
    /**
     * Get current state
     */
    State getState() const { return _state; }
    
    /**
     * Send ZeroTier data via udp2raw
     * @param remoteAddr Destination address
     * @param data ZeroTier packet data
     * @param len Data length
     * @return true on success
     */
    bool send(const InetAddress& remoteAddr, const void* data, unsigned int len);
    
    /**
     * Send raw packet (already encapsulated)
     * @param remoteAddr Destination address
     * @param packet Raw packet data
     * @param len Packet length
     * @return true on success
     */
    bool sendRaw(const InetAddress& remoteAddr, const void* packet, unsigned int len);
    
    /**
     * Receive packet
     * @param buf Buffer for received data
     * @param bufLen Buffer size
     * @param fromAddr Source address (output)
     * @param timeoutMs Timeout in milliseconds (0 = non-blocking)
     * @return Number of bytes received, 0 if no data, -1 on error
     */
    int receive(void* buf, unsigned int bufLen, InetAddress& fromAddr, int timeoutMs = 0);
    
    /**
     * Send handshake request
     * @param remoteAddr Destination address
     * @return true on success
     */
    bool sendHandshake(const InetAddress& remoteAddr);
    
    /**
     * Send handshake acknowledgment
     * @param remoteAddr Destination address
     * @return true on success
     */
    bool sendHandshakeAck(const InetAddress& remoteAddr);
    
    /**
     * Send keepalive
     * @param remoteAddr Destination address
     * @return true on success
     */
    bool sendKeepalive(const InetAddress& remoteAddr);
    
    /**
     * Send probe to test peer capability
     * @param remoteAddr Destination address
     * @return true on success
     */
    bool sendProbe(const InetAddress& remoteAddr);
    
    /**
     * Process received packet (handles handshake, keepalive, etc.)
     * @param fromAddr Source address
     * @param data Packet data
     * @param len Packet length
     * @param outBuf Output buffer for ZeroTier payload
     * @param outLen Output length
     * @return true if packet was processed (not a data packet)
     */
    bool processControlPacket(const InetAddress& fromAddr, const void* data, 
                              unsigned int len, void* outBuf, unsigned int& outLen);
    
    /**
     * Update TCP sequence numbers
     * @param seqSent Sequence number sent
     * @param ackReceived Ack received
     */
    void updateSeq(uint32_t seqSent, uint32_t ackReceived);
    
    /**
     * Get current TCP sequence number
     */
    uint32_t getSeq() const { return _tcpSeq.load(); }
    
    /**
     * Get current TCP ack number
     */
    uint32_t getAck() const { return _tcpAck.load(); }
    
    /**
     * Get statistics
     */
    Stats getStats() const { 
        Stats s;
        s.packetsSent = _packetsSent.load();
        s.packetsReceived = _packetsReceived.load();
        s.bytesSent = _bytesSent.load();
        s.bytesReceived = _bytesReceived.load();
        s.errors = _errors.load();
        s.checksumErrors = _checksumErrors.load();
        return s;
    }
    
    /**
     * Get last error message
     */
    const char* getLastError() const { return _lastError.c_str(); }
    
    /**
     * Check if raw sockets are supported on this platform
     */
    static bool isSupported();
    
    /**
     * Check if we have necessary privileges for raw sockets
     */
    static bool hasPrivileges();
    
    /**
     * Get recommended TCP ports for FakeTCP mode
     * These are commonly allowed ports
     */
    static const uint16_t* getRecommendedPorts(unsigned int& count);
    
private:
    // Platform-specific socket type
#ifdef _WIN32
    typedef SOCKET SocketType;
    static const SocketType INVALID_SOCKET_VAL = INVALID_SOCKET;
#else
    typedef int SocketType;
    static const SocketType INVALID_SOCKET_VAL = -1;
#endif
    
    // Internal methods
    bool createRawSocket(bool ipv6);
    bool bindToInterface(const InetAddress& localAddr);
    
    // Member variables
    SocketType _socket;
    std::atomic<State> _state;
    std::atomic<TcpState> _tcpState;
    Config _config;
    std::atomic<uint32_t> _tcpSeq;
    std::atomic<uint32_t> _tcpAck;
    std::atomic<uint64_t> _packetsSent;
    std::atomic<uint64_t> _packetsReceived;
    std::atomic<uint64_t> _bytesSent;
    std::atomic<uint64_t> _bytesReceived;
    std::atomic<uint64_t> _errors;
    std::atomic<uint64_t> _checksumErrors;
    std::string _lastError;
    bool _isIPv6;
};

} // namespace ZeroTier

#endif // ZT_UDP2RAW_SOCKET_HPP
