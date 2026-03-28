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
 * Udp2Raw protocol implementation
 * Encapsulates UDP traffic in FakeTCP/ICMP packets to bypass UDP firewalls
 */

#ifndef ZT_UDP2RAW_PROTOCOL_HPP
#define ZT_UDP2RAW_PROTOCOL_HPP

#include <cstdint>
#include <cstring>

namespace ZeroTier {

/**
 * Udp2Raw protocol constants and structures
 */
class Udp2RawProtocol {
public:
    // Protocol magic number
    static constexpr uint32_t MAGIC = 0x52415732; // "RAW2"
    
    // Protocol version
    static constexpr uint8_t VERSION = 1;
    
    // Packet types
    enum PacketType : uint8_t {
        PACKET_TYPE_DATA = 0x01,      // Data packet
        PACKET_TYPE_HANDSHAKE = 0x02, // Handshake request
        PACKET_TYPE_HANDSHAKE_ACK = 0x03, // Handshake response
        PACKET_TYPE_KEEPALIVE = 0x04, // Keepalive
        PACKET_TYPE_PROBE = 0x05      // Capability probe
    };
    
    // Transport modes
    enum TransportMode : uint8_t {
        MODE_FAKE_TCP = 0x01,
        MODE_ICMP = 0x02
    };
    
    // Udp2Raw packet header (followed by encrypted ZeroTier payload)
#ifdef _WIN32
#pragma pack(push, 1)
#endif
    struct
#ifdef _WIN32
#else
    __attribute__((packed))
#endif
    PacketHeader {
        uint32_t magic;       // Magic number
        uint8_t version;      // Protocol version
        uint8_t packetType;   // Packet type
        uint16_t payloadLen;  // Payload length
        uint32_t seqNum;      // TCP sequence number (for FakeTCP)
        uint32_t ackNum;      // TCP acknowledgment number
        uint8_t options;      // Option flags
        uint8_t reserved[3];  // Reserved
    };
    
    // TCP header for FakeTCP mode (20 bytes minimum)
    struct
#ifdef _WIN32
#else
    __attribute__((packed))
#endif
    TcpHeader {
        uint16_t sourcePort;
        uint16_t destPort;
        uint32_t seqNum;
        uint32_t ackNum;
        uint8_t dataOffset;   // 4 bits data offset, 4 bits reserved
        uint8_t flags;
        uint16_t windowSize;
        uint16_t checksum;
        uint16_t urgentPtr;
    };
    
    // ICMP header for ICMP mode
    struct
#ifdef _WIN32
#else
    __attribute__((packed))
#endif
    IcmpHeader {
        uint8_t type;
        uint8_t code;
        uint16_t checksum;
        uint16_t id;
        uint16_t seq;
    };
#ifdef _WIN32
#pragma pack(pop)
#endif
    
    // TCP flags
    static constexpr uint8_t TCP_FLAG_FIN = 0x01;
    static constexpr uint8_t TCP_FLAG_SYN = 0x02;
    static constexpr uint8_t TCP_FLAG_RST = 0x04;
    static constexpr uint8_t TCP_FLAG_PSH = 0x08;
    static constexpr uint8_t TCP_FLAG_ACK = 0x10;
    static constexpr uint8_t TCP_FLAG_URG = 0x20;
    
    // Option flags
    static constexpr uint8_t OPT_ENCRYPTED = 0x01;
    static constexpr uint8_t OPT_COMPRESSED = 0x02;
    
    // Header sizes
    static constexpr unsigned int HEADER_SIZE = sizeof(PacketHeader);
    static constexpr unsigned int TCP_HEADER_SIZE = sizeof(TcpHeader);
    static constexpr unsigned int ICMP_HEADER_SIZE = sizeof(IcmpHeader);
    static constexpr unsigned int MAX_PACKET_SIZE = 1500; // MTU
    static constexpr unsigned int MAX_PAYLOAD_SIZE = MAX_PACKET_SIZE - 40 - HEADER_SIZE; // IP + UDP/TCP overhead
    
    /**
     * Initialize a packet header
     */
    static void initHeader(PacketHeader* hdr, PacketType type, uint16_t payloadLen,
                          uint32_t seq = 0, uint32_t ack = 0, uint8_t options = 0);
    
    /**
     * Validate a received packet header
     * @return true if header is valid
     */
    static bool validateHeader(const PacketHeader* hdr, unsigned int totalLen);
    
    /**
     * Calculate TCP checksum
     */
    static uint16_t calculateTcpChecksum(const void* tcpHeader, unsigned int tcpLen,
                                        const uint8_t* srcIp, const uint8_t* dstIp, bool isIPv6);
    
    /**
     * Calculate ICMP checksum
     */
    static uint16_t calculateIcmpChecksum(const void* icmpData, unsigned int len);
    
    /**
     * Encapsulate ZeroTier data in FakeTCP packet
     * @param ztData ZeroTier packet data
     * @param ztLen ZeroTier packet length
     * @param outBuf Output buffer (must be large enough)
     * @param outLen Output length
     * @param srcPort Source TCP port
     * @param dstPort Destination TCP port
     * @param seq TCP sequence number
     * @param ack TCP acknowledgment number
     * @param flags TCP flags
     * @return true on success
     */
    static bool encapsulateFakeTcp(const void* ztData, unsigned int ztLen,
                                   void* outBuf, unsigned int& outLen,
                                   uint16_t srcPort, uint16_t dstPort,
                                   uint32_t seq, uint32_t ack, uint8_t flags,
                                   const uint8_t* srcIp, const uint8_t* dstIp, bool isIPv6);
    
    /**
     * Decapsulate FakeTCP packet to get ZeroTier data
     * @param rawData Raw packet data (IP packet)
     * @param rawLen Raw packet length
     * @param outBuf Output buffer
     * @param outLen Output length
     * @param outSrcPort Output source port (optional)
     * @param outDstPort Output destination port (optional)
     * @return true if valid FakeTCP with ZeroTier payload
     */
    static bool decapsulateFakeTcp(const void* rawData, unsigned int rawLen,
                                   void* outBuf, unsigned int& outLen,
                                   uint16_t* outSrcPort = nullptr,
                                   uint16_t* outDstPort = nullptr);
    
    /**
     * Encapsulate ZeroTier data in ICMP packet
     */
    static bool encapsulateIcmp(const void* ztData, unsigned int ztLen,
                               void* outBuf, unsigned int& outLen,
                               uint16_t icmpId, uint16_t icmpSeq,
                               const uint8_t* srcIp, const uint8_t* dstIp, bool isIPv6);
    
    /**
     * Decapsulate ICMP packet to get ZeroTier data
     */
    static bool decapsulateIcmp(const void* rawData, unsigned int rawLen,
                               void* outBuf, unsigned int& outLen,
                               uint16_t* outId = nullptr, uint16_t* outSeq = nullptr);
    
    /**
     * Create handshake request packet
     */
    static unsigned int createHandshake(void* buf, unsigned int bufLen,
                                        TransportMode mode, uint16_t preferredPort = 0);
    
    /**
     * Create handshake acknowledgment
     */
    static unsigned int createHandshakeAck(void* buf, unsigned int bufLen,
                                           TransportMode agreedMode, uint16_t agreedPort = 0);
    
    /**
     * Create keepalive packet
     */
    static unsigned int createKeepalive(void* buf, unsigned int bufLen, uint32_t seq, uint32_t ack);
    
    /**
     * Create probe packet to test peer capability
     */
    static unsigned int createProbe(void* buf, unsigned int bufLen);
    
    /**
     * Check if packet is a valid udp2raw probe
     */
    static bool isProbePacket(const void* data, unsigned int len);
};

} // namespace ZeroTier

#endif // ZT_UDP2RAW_PROTOCOL_HPP
