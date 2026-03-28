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

#include "PathUdp2Raw.hpp"

#include <cstring>
#include <cstdio>

namespace ZeroTier {

PathUdp2Raw::PathUdp2Raw() :
    _transportType(TRANSPORT_UDP),
    _state(UDP2RAW_DISABLED),
    _lastActivity(0),
    _lastProbeTime(0),
    _lastFailureTime(0),
    _packetsSent(0),
    _packetsReceived(0),
    _errors(0),
    _probeSent(false),
    _handshakeSent(false),
    _handshakeStartTime(0)
{
}

PathUdp2Raw::~PathUdp2Raw()
{
    shutdown();
}

bool PathUdp2Raw::isSystemSupported()
{
    return Udp2RawSocket::isSupported() && Udp2RawSocket::hasPrivileges();
}

const char* PathUdp2Raw::getTransportTypeName(TransportType type)
{
    switch (type) {
    case TRANSPORT_UDP:
        return "UDP";
    case TRANSPORT_UDP2RAW_TCP:
        return "UDP2RAW_TCP";
    case TRANSPORT_UDP2RAW_ICMP:
        return "UDP2RAW_ICMP";
    default:
        return "UNKNOWN";
    }
}

const char* PathUdp2Raw::getStateName(Udp2RawState state)
{
    switch (state) {
    case UDP2RAW_DISABLED:
        return "DISABLED";
    case UDP2RAW_PROBING:
        return "PROBING";
    case UDP2RAW_HANDSHAKE:
        return "HANDSHAKE";
    case UDP2RAW_ESTABLISHED:
        return "ESTABLISHED";
    case UDP2RAW_FAILED:
        return "FAILED";
    default:
        return "UNKNOWN";
    }
}

bool PathUdp2Raw::init(const InetAddress& localAddr, const InetAddress& remoteAddr, const Config& config)
{
    if (!config.enabled) {
        _state.store(UDP2RAW_DISABLED);
        return true;
    }
    
    if (!isSystemSupported()) {
        _state.store(UDP2RAW_FAILED);
        return false;
    }
    
    _localAddr = localAddr;
    _remoteAddr = remoteAddr;
    _config = config;
    
    // Start with UDP, will upgrade to udp2raw if probing succeeds
    _transportType.store(TRANSPORT_UDP);
    _state.store(UDP2RAW_PROBING);
    _probeSent = false;
    _handshakeSent = false;
    
    return true;
}

void PathUdp2Raw::shutdown()
{
    if (_socket) {
        _socket->close();
        _socket.reset();
    }
    _state.store(UDP2RAW_DISABLED);
    _transportType.store(TRANSPORT_UDP);
}

bool PathUdp2Raw::shouldUseUdp2Raw() const
{
    Udp2RawState state = _state.load();
    return state == UDP2RAW_ESTABLISHED;
}

bool PathUdp2Raw::send(const void* data, unsigned int len)
{
    if (!shouldUseUdp2Raw()) {
        return false;
    }
    
    if (!_socket) {
        return false;
    }
    
    bool result = _socket->send(_remoteAddr, data, len);
    if (result) {
        _packetsSent.fetch_add(1);
        updateActivity(0); // 0 means use current time
    } else {
        _errors.fetch_add(1);
    }
    
    return result;
}

int PathUdp2Raw::receive(void* buf, unsigned int bufLen, int timeoutMs)
{
    if (!shouldUseUdp2Raw()) {
        return -1;
    }
    
    if (!_socket) {
        return -1;
    }
    
    InetAddress fromAddr;
    int result = _socket->receive(buf, bufLen, fromAddr, timeoutMs);
    
    if (result > 0) {
        _packetsReceived.fetch_add(1);
        updateActivity(0);
    }
    
    return result;
}

bool PathUdp2Raw::processIncomingPacket(const void* data, unsigned int len)
{
    if (_state.load() == UDP2RAW_DISABLED) {
        return false;
    }
    
    // Check if this is a udp2raw control packet
    if (len >= Udp2RawProtocol::HEADER_SIZE) {
        const Udp2RawProtocol::PacketHeader* hdr = 
            static_cast<const Udp2RawProtocol::PacketHeader*>(data);
        
        if (Udp2RawProtocol::validateHeader(hdr, len)) {
            switch (hdr->packetType) {
            case Udp2RawProtocol::PACKET_TYPE_PROBE:
                handleProbeResponse();
                return true;
                
            case Udp2RawProtocol::PACKET_TYPE_HANDSHAKE:
                // Received handshake request, send ACK
                if (_socket) {
                    _socket->sendHandshakeAck(_remoteAddr);
                }
                return true;
                
            case Udp2RawProtocol::PACKET_TYPE_HANDSHAKE_ACK:
                // Extract mode and port from payload
                if (len >= Udp2RawProtocol::HEADER_SIZE + 4) {
                    const uint8_t* payload = 
                        static_cast<const uint8_t*>(data) + Udp2RawProtocol::HEADER_SIZE;
                    Udp2RawProtocol::TransportMode mode = 
                        static_cast<Udp2RawProtocol::TransportMode>(payload[0]);
                    uint16_t port = (payload[2] << 8) | payload[3];
                    handleHandshakeAck(mode, port);
                }
                return true;
                
            case Udp2RawProtocol::PACKET_TYPE_KEEPALIVE:
                // Just update activity
                updateActivity(0);
                return true;
                
            default:
                break;
            }
        }
    }
    
    return false;
}

void PathUdp2Raw::startProbe()
{
    if (_state.load() != UDP2RAW_PROBING) {
        return;
    }
    
    if (_probeSent) {
        return;
    }
    
    // Create socket if not exists
    if (!_socket) {
        _socket = std::make_unique<Udp2RawSocket>();
        
        Udp2RawSocket::Config socketConfig;
        socketConfig.mode = _config.preferredMode;
        socketConfig.localPort = _config.localPort;
        socketConfig.remotePort = _config.remotePort;
        socketConfig.icmpId = _config.icmpId;
        
        if (!_socket->open(_localAddr, socketConfig)) {
            _socket.reset();
            markFailed();
            return;
        }
    }
    
    // Send probe
    if (_socket->sendProbe(_remoteAddr)) {
        _probeSent = true;
        _lastProbeTime.store(0); // Will be set to current time by caller
    }
}

void PathUdp2Raw::startHandshake()
{
    if (_state.load() != UDP2RAW_PROBING) {
        return;
    }
    
    _state.store(UDP2RAW_HANDSHAKE);
    _handshakeStartTime = 0; // Will be set by caller
    _handshakeSent = false;
    
    if (!_socket) {
        markFailed();
        return;
    }
    
    if (_socket->sendHandshake(_remoteAddr)) {
        _handshakeSent = true;
    } else {
        markFailed();
    }
}

void PathUdp2Raw::handleProbeResponse()
{
    if (_state.load() == UDP2RAW_PROBING) {
        // Peer supports udp2raw, start handshake
        startHandshake();
    }
}

void PathUdp2Raw::handleHandshakeAck(Udp2RawProtocol::TransportMode mode, uint16_t port)
{
    if (_state.load() != UDP2RAW_HANDSHAKE) {
        return;
    }
    
    // Update transport type based on agreed mode
    if (mode == Udp2RawProtocol::MODE_FAKE_TCP) {
        _transportType.store(TRANSPORT_UDP2RAW_TCP);
    } else if (mode == Udp2RawProtocol::MODE_ICMP) {
        _transportType.store(TRANSPORT_UDP2RAW_ICMP);
    }
    
    // Update remote port if agreed
    if (port != 0) {
        _config.remotePort = port;
    }
    
    _state.store(UDP2RAW_ESTABLISHED);
    updateActivity(0);
}

void PathUdp2Raw::markFailed()
{
    _state.store(UDP2RAW_FAILED);
    _lastFailureTime.store(0); // Will be set to current time by caller
    
    if (_socket) {
        _socket->close();
    }
    
    _transportType.store(TRANSPORT_UDP);
}

void PathUdp2Raw::updateActivity(int64_t now)
{
    if (now == 0) {
        // Use current time - in real implementation would get from RuntimeEnvironment
        // For now just use a placeholder
        now = 1;
    }
    _lastActivity.store(now);
}

bool PathUdp2Raw::shouldRetry(int64_t now) const
{
    if (_state.load() != UDP2RAW_FAILED) {
        return false;
    }
    
    int64_t lastFailure = _lastFailureTime.load();
    if (lastFailure == 0) {
        return true; // Never failed before, can retry
    }
    
    // Retry after fallback delay
    return (now - lastFailure) > _config.fallbackDelayMs;
}

void PathUdp2Raw::getStats(uint64_t& sent, uint64_t& received, uint64_t& errors) const
{
    sent = _packetsSent.load();
    received = _packetsReceived.load();
    errors = _errors.load();
}

} // namespace ZeroTier