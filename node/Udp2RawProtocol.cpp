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

#include "Udp2RawProtocol.hpp"

#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace ZeroTier {

void Udp2RawProtocol::initHeader(PacketHeader* hdr, PacketType type, uint16_t payloadLen,
                                 uint32_t seq, uint32_t ack, uint8_t options)
{
    hdr->magic = MAGIC;
    hdr->version = VERSION;
    hdr->packetType = static_cast<uint8_t>(type);
    hdr->payloadLen = payloadLen;
    hdr->seqNum = seq;
    hdr->ackNum = ack;
    hdr->options = options;
    hdr->reserved[0] = 0;
    hdr->reserved[1] = 0;
    hdr->reserved[2] = 0;
}

bool Udp2RawProtocol::validateHeader(const PacketHeader* hdr, unsigned int totalLen)
{
    if (totalLen < HEADER_SIZE) {
        return false;
    }
    
    if (hdr->magic != MAGIC) {
        return false;
    }
    
    if (hdr->version != VERSION) {
        return false;
    }
    
    if (hdr->packetType < PACKET_TYPE_DATA || hdr->packetType > PACKET_TYPE_PROBE) {
        return false;
    }
    
    if (totalLen < HEADER_SIZE + hdr->payloadLen) {
        return false;
    }
    
    return true;
}

uint16_t Udp2RawProtocol::calculateTcpChecksum(const void* tcpHeader, unsigned int tcpLen,
                                               const uint8_t* srcIp, const uint8_t* dstIp, bool isIPv6)
{
    uint32_t sum = 0;
    const uint16_t* data = static_cast<const uint16_t*>(tcpHeader);
    
    // Sum TCP header and data
    for (unsigned int i = 0; i < tcpLen / 2; ++i) {
        sum += data[i];
    }
    
    // Add leftover byte if odd length
    if (tcpLen % 2) {
        sum += static_cast<const uint8_t*>(tcpHeader)[tcpLen - 1] << 8;
    }
    
    // Add pseudo-header
    if (isIPv6) {
        // IPv6 pseudo-header: src (16 bytes) + dst (16 bytes) + length (4 bytes) + next header (4 bytes)
        for (int i = 0; i < 8; ++i) {
            sum += reinterpret_cast<const uint16_t*>(srcIp)[i];
        }
        for (int i = 0; i < 8; ++i) {
            sum += reinterpret_cast<const uint16_t*>(dstIp)[i];
        }
        sum += htons(static_cast<uint16_t>(tcpLen));
        sum += htons(static_cast<uint16_t>(tcpLen >> 16));
        sum += htons(6); // TCP protocol number
    } else {
        // IPv4 pseudo-header: src (4 bytes) + dst (4 bytes) + zero (1 byte) + protocol (1 byte) + length (2 bytes)
        sum += reinterpret_cast<const uint16_t*>(srcIp)[0];
        sum += reinterpret_cast<const uint16_t*>(srcIp)[1];
        sum += reinterpret_cast<const uint16_t*>(dstIp)[0];
        sum += reinterpret_cast<const uint16_t*>(dstIp)[1];
        sum += htons(6); // TCP protocol number
        sum += htons(static_cast<uint16_t>(tcpLen));
    }
    
    // Fold 32-bit sum to 16-bit
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return static_cast<uint16_t>(~sum);
}

uint16_t Udp2RawProtocol::calculateIcmpChecksum(const void* icmpData, unsigned int len)
{
    uint32_t sum = 0;
    const uint16_t* data = static_cast<const uint16_t*>(icmpData);
    
    for (unsigned int i = 0; i < len / 2; ++i) {
        sum += data[i];
    }
    
    if (len % 2) {
        sum += static_cast<const uint8_t*>(icmpData)[len - 1] << 8;
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return static_cast<uint16_t>(~sum);
}

bool Udp2RawProtocol::encapsulateFakeTcp(const void* ztData, unsigned int ztLen,
                                         void* outBuf, unsigned int& outLen,
                                         uint16_t srcPort, uint16_t dstPort,
                                         uint32_t seq, uint32_t ack, uint8_t flags,
                                         const uint8_t* srcIp, const uint8_t* dstIp, bool isIPv6)
{
    if (ztLen > MAX_PAYLOAD_SIZE) {
        return false;
    }
    
    // Build udp2raw packet header + payload
    uint8_t* buf = static_cast<uint8_t*>(outBuf);
    unsigned int offset = 0;
    
    // IP header will be added by kernel when using raw socket
    // We build TCP header + payload
    
    TcpHeader* tcp = reinterpret_cast<TcpHeader*>(buf + offset);
    tcp->sourcePort = htons(srcPort);
    tcp->destPort = htons(dstPort);
    tcp->seqNum = htonl(seq);
    tcp->ackNum = htonl(ack);
    tcp->dataOffset = (5 << 4); // 5 * 4 = 20 bytes, no options
    tcp->flags = flags;
    tcp->windowSize = htons(65535);
    tcp->urgentPtr = 0;
    
    offset += TCP_HEADER_SIZE;
    
    // Add udp2raw header
    PacketHeader* hdr = reinterpret_cast<PacketHeader*>(buf + offset);
    initHeader(hdr, PACKET_TYPE_DATA, static_cast<uint16_t>(ztLen), seq, ack, 0);
    offset += HEADER_SIZE;
    
    // Copy payload
    memcpy(buf + offset, ztData, ztLen);
    offset += ztLen;
    
    // Calculate TCP checksum (includes pseudo-header)
    tcp->checksum = 0;
    tcp->checksum = calculateTcpChecksum(tcp, offset, srcIp, dstIp, isIPv6);
    
    outLen = offset;
    return true;
}

bool Udp2RawProtocol::decapsulateFakeTcp(const void* rawData, unsigned int rawLen,
                                         void* outBuf, unsigned int& outLen,
                                         uint16_t* outSrcPort, uint16_t* outDstPort)
{
    if (rawLen < 20 + TCP_HEADER_SIZE + HEADER_SIZE) {
        return false; // Too small for IP + TCP + udp2raw header
    }
    
    const uint8_t* data = static_cast<const uint8_t*>(rawData);
    
    // Parse IP header to find TCP header
    uint8_t ipVersion = (data[0] >> 4) & 0x0F;
    unsigned int ipHeaderLen;
    bool isIPv6;
    
    if (ipVersion == 4) {
        ipHeaderLen = (data[0] & 0x0F) * 4;
        isIPv6 = false;
    } else if (ipVersion == 6) {
        ipHeaderLen = 40;
        isIPv6 = true;
    } else {
        return false;
    }
    
    if (rawLen < ipHeaderLen + TCP_HEADER_SIZE + HEADER_SIZE) {
        return false;
    }
    
    const TcpHeader* tcp = reinterpret_cast<const TcpHeader*>(data + ipHeaderLen);
    unsigned int tcpDataOffset = (tcp->dataOffset >> 4) * 4;
    
    if (tcpDataOffset < TCP_HEADER_SIZE) {
        return false;
    }
    
    // Extract ports
    if (outSrcPort) {
        *outSrcPort = ntohs(tcp->sourcePort);
    }
    if (outDstPort) {
        *outDstPort = ntohs(tcp->destPort);
    }
    
    // Find udp2raw header in TCP payload
    const uint8_t* tcpPayload = reinterpret_cast<const uint8_t*>(tcp) + tcpDataOffset;
    unsigned int tcpPayloadLen = rawLen - ipHeaderLen - tcpDataOffset;
    
    if (tcpPayloadLen < HEADER_SIZE) {
        return false;
    }
    
    const PacketHeader* hdr = reinterpret_cast<const PacketHeader*>(tcpPayload);
    
    if (!validateHeader(hdr, tcpPayloadLen)) {
        return false;
    }
    
    // Copy ZeroTier payload
    const uint8_t* payload = tcpPayload + HEADER_SIZE;
    outLen = hdr->payloadLen;
    memcpy(outBuf, payload, outLen);
    
    return true;
}

bool Udp2RawProtocol::encapsulateIcmp(const void* ztData, unsigned int ztLen,
                                      void* outBuf, unsigned int& outLen,
                                      uint16_t icmpId, uint16_t icmpSeq,
                                      const uint8_t* srcIp, const uint8_t* dstIp, bool isIPv6)
{
    if (ztLen > MAX_PAYLOAD_SIZE) {
        return false;
    }
    
    uint8_t* buf = static_cast<uint8_t*>(outBuf);
    unsigned int offset = 0;
    
    // Build ICMP header + udp2raw header + payload
    IcmpHeader* icmp = reinterpret_cast<IcmpHeader*>(buf + offset);
    icmp->type = isIPv6 ? 128 : 8; // Echo request (128 for IPv6, 8 for IPv4)
    icmp->code = 0;
    icmp->id = htons(icmpId);
    icmp->seq = htons(icmpSeq);
    icmp->checksum = 0;
    
    offset += ICMP_HEADER_SIZE;
    
    // Add udp2raw header
    PacketHeader* hdr = reinterpret_cast<PacketHeader*>(buf + offset);
    initHeader(hdr, PACKET_TYPE_DATA, static_cast<uint16_t>(ztLen), 0, 0, 0);
    offset += HEADER_SIZE;
    
    // Copy payload
    memcpy(buf + offset, ztData, ztLen);
    offset += ztLen;
    
    // Calculate ICMP checksum
    icmp->checksum = calculateIcmpChecksum(icmp, offset);
    
    outLen = offset;
    return true;
}

bool Udp2RawProtocol::decapsulateIcmp(const void* rawData, unsigned int rawLen,
                                      void* outBuf, unsigned int& outLen,
                                      uint16_t* outId, uint16_t* outSeq)
{
    if (rawLen < 20 + ICMP_HEADER_SIZE + HEADER_SIZE) {
        return false;
    }
    
    const uint8_t* data = static_cast<const uint8_t*>(rawData);
    
    // Parse IP header
    uint8_t ipVersion = (data[0] >> 4) & 0x0F;
    unsigned int ipHeaderLen;
    
    if (ipVersion == 4) {
        ipHeaderLen = (data[0] & 0x0F) * 4;
    } else if (ipVersion == 6) {
        ipHeaderLen = 40;
    } else {
        return false;
    }
    
    if (rawLen < ipHeaderLen + ICMP_HEADER_SIZE + HEADER_SIZE) {
        return false;
    }
    
    const IcmpHeader* icmp = reinterpret_cast<const IcmpHeader*>(data + ipHeaderLen);
    
    // Validate ICMP type (echo request or reply)
    uint8_t expectedType = (ipVersion == 6) ? 128 : 8; // Echo request
    uint8_t expectedReply = (ipVersion == 6) ? 129 : 0; // Echo reply
    
    if (icmp->type != expectedType && icmp->type != expectedReply) {
        return false;
    }
    
    if (outId) {
        *outId = ntohs(icmp->id);
    }
    if (outSeq) {
        *outSeq = ntohs(icmp->seq);
    }
    
    // Find udp2raw header in ICMP payload
    const uint8_t* icmpPayload = reinterpret_cast<const uint8_t*>(icmp) + ICMP_HEADER_SIZE;
    unsigned int icmpPayloadLen = rawLen - ipHeaderLen - ICMP_HEADER_SIZE;
    
    if (icmpPayloadLen < HEADER_SIZE) {
        return false;
    }
    
    const PacketHeader* hdr = reinterpret_cast<const PacketHeader*>(icmpPayload);
    
    if (!validateHeader(hdr, icmpPayloadLen)) {
        return false;
    }
    
    // Copy ZeroTier payload
    const uint8_t* payload = icmpPayload + HEADER_SIZE;
    outLen = hdr->payloadLen;
    memcpy(outBuf, payload, outLen);
    
    return true;
}

unsigned int Udp2RawProtocol::createHandshake(void* buf, unsigned int bufLen,
                                              TransportMode mode, uint16_t preferredPort)
{
    if (bufLen < HEADER_SIZE + sizeof(uint16_t) + sizeof(uint8_t)) {
        return 0;
    }
    
    PacketHeader* hdr = static_cast<PacketHeader*>(buf);
    initHeader(hdr, PACKET_TYPE_HANDSHAKE, sizeof(uint16_t) + sizeof(uint8_t), 0, 0, 0);
    
    uint8_t* payload = reinterpret_cast<uint8_t*>(hdr) + HEADER_SIZE;
    payload[0] = static_cast<uint8_t>(mode);
    payload[1] = 0; // Reserved
    *reinterpret_cast<uint16_t*>(payload + 2) = htons(preferredPort);
    
    return HEADER_SIZE + sizeof(uint16_t) + sizeof(uint8_t);
}

unsigned int Udp2RawProtocol::createHandshakeAck(void* buf, unsigned int bufLen,
                                                TransportMode agreedMode, uint16_t agreedPort)
{
    if (bufLen < HEADER_SIZE + sizeof(uint16_t) + sizeof(uint8_t)) {
        return 0;
    }
    
    PacketHeader* hdr = static_cast<PacketHeader*>(buf);
    initHeader(hdr, PACKET_TYPE_HANDSHAKE_ACK, sizeof(uint16_t) + sizeof(uint8_t), 0, 0, 0);
    
    uint8_t* payload = reinterpret_cast<uint8_t*>(hdr) + HEADER_SIZE;
    payload[0] = static_cast<uint8_t>(agreedMode);
    payload[1] = 0; // Reserved
    *reinterpret_cast<uint16_t*>(payload + 2) = htons(agreedPort);
    
    return HEADER_SIZE + sizeof(uint16_t) + sizeof(uint8_t);
}

unsigned int Udp2RawProtocol::createKeepalive(void* buf, unsigned int bufLen, uint32_t seq, uint32_t ack)
{
    if (bufLen < HEADER_SIZE) {
        return 0;
    }
    
    PacketHeader* hdr = static_cast<PacketHeader*>(buf);
    initHeader(hdr, PACKET_TYPE_KEEPALIVE, 0, seq, ack, 0);
    
    return HEADER_SIZE;
}

unsigned int Udp2RawProtocol::createProbe(void* buf, unsigned int bufLen)
{
    if (bufLen < HEADER_SIZE) {
        return 0;
    }
    
    PacketHeader* hdr = static_cast<PacketHeader*>(buf);
    initHeader(hdr, PACKET_TYPE_PROBE, 0, 0, 0, 0);
    
    return HEADER_SIZE;
}

bool Udp2RawProtocol::isProbePacket(const void* data, unsigned int len)
{
    if (len < HEADER_SIZE) {
        return false;
    }
    
    const PacketHeader* hdr = static_cast<const PacketHeader*>(data);
    return validateHeader(hdr, len) && hdr->packetType == PACKET_TYPE_PROBE;
}

} // namespace ZeroTier
