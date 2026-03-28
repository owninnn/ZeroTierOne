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

#include "Udp2RawSocket.hpp"

#include <cstring>
#include <cstdio>

#ifndef _WIN32
#include <netinet/ip_icmp.h>
#include <poll.h>
#endif

namespace ZeroTier {

// Recommended TCP ports for FakeTCP (commonly allowed)
static const uint16_t RECOMMENDED_PORTS[] = {
    80,     // HTTP
    443,    // HTTPS
    8080,   // HTTP proxy
    8443,   // HTTPS alt
    53,     // DNS (TCP)
    22,     // SSH
    25,     // SMTP
    587,    // SMTP submission
    993,    // IMAPS
    995     // POP3S
};

Udp2RawSocket::Udp2RawSocket() :
    _socket(INVALID_SOCKET_VAL),
    _state(STATE_CLOSED),
    _tcpState(TCP_CLOSED),
    _tcpSeq(0),
    _tcpAck(0),
    _isIPv6(false)
{
}

Udp2RawSocket::~Udp2RawSocket()
{
    close();
}

bool Udp2RawSocket::isSupported()
{
#ifdef _WIN32
    // Windows requires WinDivert or similar
    return false;
#else
    return true;
#endif
}

bool Udp2RawSocket::hasPrivileges()
{
#ifdef _WIN32
    // Check for admin privileges
    // TODO: Implement for Windows
    return false;
#else
    // Check if we can create raw socket
    int sock = ::socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (sock < 0) {
        return false;
    }
    ::close(sock);
    return true;
#endif
}

const uint16_t* Udp2RawSocket::getRecommendedPorts(unsigned int& count)
{
    count = sizeof(RECOMMENDED_PORTS) / sizeof(RECOMMENDED_PORTS[0]);
    return RECOMMENDED_PORTS;
}

bool Udp2RawSocket::createRawSocket(bool ipv6)
{
#ifdef _WIN32
    // Windows raw socket support using WinDivert would go here
    // For now, just set state to error gracefully
    _lastError = "Raw sockets not supported on Windows (WinDivert required)";
    _state = STATE_ERROR;
    return false;
#else
    _isIPv6 = ipv6;
    
    int domain = ipv6 ? AF_INET6 : AF_INET;
    int protocol = ipv6 ? IPPROTO_IPV6 : IPPROTO_IP;
    
    // Create raw socket
    if (_config.mode == Udp2RawProtocol::MODE_FAKE_TCP) {
        _socket = ::socket(domain, SOCK_RAW, IPPROTO_TCP);
    } else {
        _socket = ::socket(domain, SOCK_RAW, IPPROTO_ICMP);
        if (_isIPv6) {
            _socket = ::socket(domain, SOCK_RAW, IPPROTO_ICMPV6);
        }
    }
    
    if (_socket < 0) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Failed to create raw socket: %s", strerror(errno));
        _lastError = buf;
        return false;
    }
    
    // Set non-blocking
    int flags = fcntl(_socket, F_GETFL, 0);
    if (flags < 0) {
        close();
        _lastError = "Failed to get socket flags";
        return false;
    }
    if (fcntl(_socket, F_SETFL, flags | O_NONBLOCK) < 0) {
        close();
        _lastError = "Failed to set non-blocking";
        return false;
    }
    
    // Enable IP_HDRINCL for custom IP headers (FakeTCP mode)
    if (_config.mode == Udp2RawProtocol::MODE_FAKE_TCP) {
        int on = 1;
        if (setsockopt(_socket, protocol, IP_HDRINCL, &on, sizeof(on)) < 0) {
            // Not fatal, kernel will construct IP header
        }
    }
    
    return true;
#endif
}

bool Udp2RawSocket::bindToInterface(const InetAddress& localAddr)
{
#ifdef _WIN32
    (void)localAddr;
    return false;
#else
    if (_socket < 0) {
        return false;
    }
    
    // For raw sockets, binding is optional but can be used to
    // specify which interface to use
    if (localAddr.ss_family != AF_UNSPEC) {
        // Bind to specific address if provided
        socklen_t addrLen = (localAddr.ss_family == AF_INET6) ? 
                           sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);
        if (::bind(_socket, reinterpret_cast<const struct sockaddr*>(&localAddr), addrLen) < 0) {
            char buf[256];
            snprintf(buf, sizeof(buf), "Failed to bind: %s", strerror(errno));
            _lastError = buf;
            return false;
        }
    }
    
    return true;
#endif
}

bool Udp2RawSocket::open(const InetAddress& localAddr, const Config& config)
{
    if (_state != STATE_CLOSED) {
        close();
    }
    
    _state = STATE_OPENING;
    _config = config;
    _tcpSeq = config.tcpSeq;
    _tcpAck = config.tcpAck;
    _tcpState = TCP_CLOSED;
    
    bool ipv6 = (localAddr.ss_family == AF_INET6);
    
    if (!createRawSocket(ipv6)) {
        _state = STATE_ERROR;
        return false;
    }
    
    if (!bindToInterface(localAddr)) {
        _state = STATE_ERROR;
        return false;
    }
    
    _state = STATE_OPEN;
    return true;
}

void Udp2RawSocket::close()
{
    if (_socket != INVALID_SOCKET_VAL) {
#ifdef _WIN32
        ::closesocket(_socket);
#else
        ::close(_socket);
#endif
        _socket = INVALID_SOCKET_VAL;
    }
    _state = STATE_CLOSED;
    _tcpState = TCP_CLOSED;
}

bool Udp2RawSocket::send(const InetAddress& remoteAddr, const void* data, unsigned int len)
{
    if (_state != STATE_OPEN) {
        _lastError = "Socket not open";
        return false;
    }
    
    if (len > Udp2RawProtocol::MAX_PAYLOAD_SIZE) {
        _lastError = "Payload too large";
        return false;
    }
    
    // Build packet
    uint8_t packet[Udp2RawProtocol::MAX_PACKET_SIZE];
    unsigned int packetLen = 0;
    
    // Get IP addresses
    uint8_t srcIp[16] = {0};
    uint8_t dstIp[16] = {0};
    
    if (_isIPv6) {
        // TODO: Get local IPv6 address
    } else {
        // Use 0.0.0.0 as source (kernel will fill in)
        // Or try to get from bound address
    }
    
    if (remoteAddr.ss_family == AF_INET) {
        memcpy(dstIp, &reinterpret_cast<const struct sockaddr_in*>(&remoteAddr)->sin_addr, 4);
    } else if (remoteAddr.ss_family == AF_INET6) {
        memcpy(dstIp, &reinterpret_cast<const struct sockaddr_in6*>(&remoteAddr)->sin6_addr, 16);
    }
    
    if (_config.mode == Udp2RawProtocol::MODE_FAKE_TCP) {
        // Build FakeTCP packet
        uint32_t seq = _tcpSeq.fetch_add(len + 1);
        uint32_t ack = _tcpAck.load();
        
        if (!Udp2RawProtocol::encapsulateFakeTcp(
                data, len, packet, packetLen,
                _config.localPort, _config.remotePort,
                seq, ack,
                Udp2RawProtocol::TCP_FLAG_ACK | Udp2RawProtocol::TCP_FLAG_PSH,
                srcIp, dstIp, _isIPv6)) {
            _lastError = "Failed to encapsulate FakeTCP";
            return false;
        }
    } else {
        // Build ICMP packet
        static uint16_t icmpSeqCounter = 0;
        uint16_t icmpSeq = icmpSeqCounter++;
        
        if (!Udp2RawProtocol::encapsulateIcmp(
                data, len, packet, packetLen,
                _config.icmpId, icmpSeq,
                srcIp, dstIp, _isIPv6)) {
            _lastError = "Failed to encapsulate ICMP";
            return false;
        }
    }
    
    return sendRaw(remoteAddr, packet, packetLen);
}

bool Udp2RawSocket::sendRaw(const InetAddress& remoteAddr, const void* packet, unsigned int len)
{
    if (_socket == INVALID_SOCKET_VAL) {
        return false;
    }
    
#ifdef _WIN32
    (void)remoteAddr;
    (void)packet;
    (void)len;
    return false;
#else
    ssize_t sent;
    if (remoteAddr.ss_family == AF_INET6) {
        sent = ::sendto(_socket, packet, len, 0,
                       reinterpret_cast<const struct sockaddr*>(&remoteAddr),
                       sizeof(struct sockaddr_in6));
    } else {
        sent = ::sendto(_socket, packet, len, 0,
                       reinterpret_cast<const struct sockaddr*>(&remoteAddr),
                       sizeof(struct sockaddr_in));
    }
    
    if (sent < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            char buf[256];
            snprintf(buf, sizeof(buf), "sendto failed: %s", strerror(errno));
            _lastError = buf;
            _errors.fetch_add(1);
        }
        return false;
    }
    
    _packetsSent.fetch_add(1);
    _bytesSent.fetch_add(sent);
    return true;
#endif
}

int Udp2RawSocket::receive(void* buf, unsigned int bufLen, InetAddress& fromAddr, int timeoutMs)
{
    if (_socket == INVALID_SOCKET_VAL) {
        return -1;
    }
    
#ifdef _WIN32
    (void)buf;
    (void)bufLen;
    (void)fromAddr;
    (void)timeoutMs;
    return -1;
#else
    // Use poll for timeout
    if (timeoutMs >= 0) {
        struct pollfd pfd;
        pfd.fd = _socket;
        pfd.events = POLLIN;
        pfd.revents = 0;
        
        int ret = ::poll(&pfd, 1, timeoutMs);
        if (ret < 0) {
            _errors.fetch_add(1);
            return -1;
        }
        if (ret == 0) {
            return 0; // Timeout
        }
    }
    
    // Receive packet
    uint8_t rawBuf[Udp2RawProtocol::MAX_PACKET_SIZE];
    struct sockaddr_storage ss;
    socklen_t ssLen = sizeof(ss);
    
    ssize_t received = ::recvfrom(_socket, rawBuf, sizeof(rawBuf), 0,
                                  reinterpret_cast<struct sockaddr*>(&ss), &ssLen);
    
    if (received < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            _errors.fetch_add(1);
        }
        return -1;
    }
    
    // Copy source address
    memcpy(&fromAddr, &ss, sizeof(ss));
    
    // Decapsulate
    uint8_t decapBuf[Udp2RawProtocol::MAX_PAYLOAD_SIZE];
    unsigned int decapLen = 0;
    bool success = false;
    
    if (_config.mode == Udp2RawProtocol::MODE_FAKE_TCP) {
        success = Udp2RawProtocol::decapsulateFakeTcp(
            rawBuf, received, decapBuf, decapLen, nullptr, nullptr);
    } else {
        success = Udp2RawProtocol::decapsulateIcmp(
            rawBuf, received, decapBuf, decapLen, nullptr, nullptr);
    }
    
    if (!success) {
        // Might be a control packet or invalid
        // Try to process as control packet
        if (processControlPacket(fromAddr, rawBuf, received, buf, decapLen)) {
            return 0; // Control packet processed, no data
        }
        return 0; // Invalid packet
    }
    
    // Copy payload to output buffer
    if (decapLen > bufLen) {
        decapLen = bufLen;
    }
    memcpy(buf, decapBuf, decapLen);
    
    _packetsReceived.fetch_add(1);
    _bytesReceived.fetch_add(decapLen);
    
    return static_cast<int>(decapLen);
#endif
}

bool Udp2RawSocket::sendHandshake(const InetAddress& remoteAddr)
{
    uint8_t buf[256];
    unsigned int len = Udp2RawProtocol::createHandshake(
        buf, sizeof(buf), _config.mode, _config.remotePort);
    
    if (len == 0) {
        return false;
    }
    
    // Send as UDP first to discover path
    // Then upgrade to raw socket
    _tcpState = TCP_SYN_SENT;
    return true;
}

bool Udp2RawSocket::sendHandshakeAck(const InetAddress& remoteAddr)
{
    uint8_t buf[256];
    unsigned int len = Udp2RawProtocol::createHandshakeAck(
        buf, sizeof(buf), _config.mode, _config.remotePort);
    
    if (len == 0) {
        return false;
    }
    
    _tcpState = TCP_ESTABLISHED;
    return true;
}

bool Udp2RawSocket::sendKeepalive(const InetAddress& remoteAddr)
{
    uint8_t buf[256];
    unsigned int len = Udp2RawProtocol::createKeepalive(
        buf, sizeof(buf), _tcpSeq.load(), _tcpAck.load());
    
    if (len == 0) {
        return false;
    }
    
    return sendRaw(remoteAddr, buf, len);
}

bool Udp2RawSocket::sendProbe(const InetAddress& remoteAddr)
{
    uint8_t buf[256];
    unsigned int len = Udp2RawProtocol::createProbe(buf, sizeof(buf));
    
    if (len == 0) {
        return false;
    }
    
    return sendRaw(remoteAddr, buf, len);
}

bool Udp2RawSocket::processControlPacket(const InetAddress& fromAddr, const void* data,
                                         unsigned int len, void* outBuf, unsigned int& outLen)
{
    if (len < Udp2RawProtocol::HEADER_SIZE) {
        return false;
    }
    
    const Udp2RawProtocol::PacketHeader* hdr = 
        static_cast<const Udp2RawProtocol::PacketHeader*>(data);
    
    if (!Udp2RawProtocol::validateHeader(hdr, len)) {
        return false;
    }
    
    switch (hdr->packetType) {
    case Udp2RawProtocol::PACKET_TYPE_HANDSHAKE:
        // Received handshake request, send ACK
        sendHandshakeAck(fromAddr);
        return true;
        
    case Udp2RawProtocol::PACKET_TYPE_HANDSHAKE_ACK:
        // Received handshake ACK, connection established
        _tcpState = TCP_ESTABLISHED;
        return true;
        
    case Udp2RawProtocol::PACKET_TYPE_KEEPALIVE:
        // Update ack number
        if (hdr->seqNum > _tcpAck.load()) {
            _tcpAck.store(hdr->seqNum);
        }
        return true;
        
    case Udp2RawProtocol::PACKET_TYPE_PROBE:
        // Received probe, could respond with capabilities
        // For now, just acknowledge
        return true;
        
    default:
        return false;
    }
}

void Udp2RawSocket::updateSeq(uint32_t seqSent, uint32_t ackReceived)
{
    _tcpSeq.store(seqSent);
    if (ackReceived > _tcpAck.load()) {
        _tcpAck.store(ackReceived);
    }
}

} // namespace ZeroTier
