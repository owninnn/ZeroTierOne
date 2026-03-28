/*
 * ZeroTier One - Udp2Raw Protocol Unit Tests
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include "Udp2RawProtocol.hpp"

using namespace ZeroTier;

// Test helper macros
#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s at line %d\n", #cond, __LINE__); \
        return 1; \
    } \
} while(0)

#define TEST_ASSERT_EQ(a, b) TEST_ASSERT((a) == (b))
#define TEST_ASSERT_NE(a, b) TEST_ASSERT((a) != (b))

// Test 1: Header initialization and validation
int test_header_basic()
{
    printf("Test: Header basic initialization and validation...\n");
    
    Udp2RawProtocol::PacketHeader hdr;
    Udp2RawProtocol::initHeader(&hdr, Udp2RawProtocol::PACKET_TYPE_DATA, 100, 12345, 67890, 0);
    
    TEST_ASSERT_EQ(hdr.magic, Udp2RawProtocol::MAGIC);
    TEST_ASSERT_EQ(hdr.version, Udp2RawProtocol::VERSION);
    TEST_ASSERT_EQ(hdr.packetType, Udp2RawProtocol::PACKET_TYPE_DATA);
    TEST_ASSERT_EQ(hdr.payloadLen, 100);
    TEST_ASSERT_EQ(hdr.seqNum, 12345);
    TEST_ASSERT_EQ(hdr.ackNum, 67890);
    
    // Validate header
    TEST_ASSERT(Udp2RawProtocol::validateHeader(&hdr, sizeof(hdr) + 100));
    
    // Invalid: too small
    TEST_ASSERT(!Udp2RawProtocol::validateHeader(&hdr, sizeof(hdr) - 1));
    
    // Invalid: wrong magic
    hdr.magic = 0xDEADBEEF;
    TEST_ASSERT(!Udp2RawProtocol::validateHeader(&hdr, sizeof(hdr) + 100));
    hdr.magic = Udp2RawProtocol::MAGIC;
    
    // Invalid: wrong version
    hdr.version = 99;
    TEST_ASSERT(!Udp2RawProtocol::validateHeader(&hdr, sizeof(hdr) + 100));
    hdr.version = Udp2RawProtocol::VERSION;
    
    // Invalid: payload too large
    hdr.payloadLen = 2000;
    TEST_ASSERT(!Udp2RawProtocol::validateHeader(&hdr, sizeof(hdr) + 100));
    
    printf("  PASSED\n");
    return 0;
}

// Test 2: Checksum calculation
int test_checksum()
{
    printf("Test: Checksum calculation...\n");
    
    // Test ICMP checksum
    uint8_t icmpData[] = {0x08, 0x00, 0x00, 0x00, 0x12, 0x34, 0x00, 0x01, 0xAA, 0xBB, 0xCC, 0xDD};
    uint16_t checksum = Udp2RawProtocol::calculateIcmpChecksum(icmpData, sizeof(icmpData));
    
    // Verify checksum by adding it back
    uint32_t sum = checksum;
    for (size_t i = 0; i < sizeof(icmpData) / 2; ++i) {
        sum += reinterpret_cast<uint16_t*>(icmpData)[i];
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    TEST_ASSERT_EQ(static_cast<uint16_t>(sum), 0xFFFF);
    
    printf("  PASSED\n");
    return 0;
}

// Test 3: FakeTCP encapsulation/decapsulation
int test_faketcp_encap_decap()
{
    printf("Test: FakeTCP encapsulation/decapsulation...\n");
    
    // Test data
    const char* ztData = "Hello, ZeroTier over FakeTCP!";
    unsigned int ztLen = strlen(ztData) + 1;
    
    uint8_t outBuf[2048];
    unsigned int outLen = 0;
    
    // IPv4 addresses
    uint8_t srcIp[4] = {192, 168, 1, 1};
    uint8_t dstIp[4] = {192, 168, 1, 2};
    
    // Encapsulate
    bool result = Udp2RawProtocol::encapsulateFakeTcp(
        ztData, ztLen,
        outBuf, outLen,
        12345, 443, // srcPort, dstPort
        1000, 500,  // seq, ack
        Udp2RawProtocol::TCP_FLAG_ACK | Udp2RawProtocol::TCP_FLAG_PSH,
        srcIp, dstIp, false
    );
    TEST_ASSERT(result);
    TEST_ASSERT(outLen > 0);
    TEST_ASSERT(outLen >= Udp2RawProtocol::TCP_HEADER_SIZE + Udp2RawProtocol::HEADER_SIZE + ztLen);
    
    // Build a fake IP packet for decapsulation
    uint8_t ipPacket[2048];
    // IP header (minimal IPv4)
    ipPacket[0] = 0x45; // Version 4, IHL 5
    ipPacket[1] = 0x00; // DSCP/ECN
    uint16_t totalLen = htons(20 + outLen);
    memcpy(ipPacket + 2, &totalLen, 2);
    ipPacket[4] = 0x00; ipPacket[5] = 0x01; // ID
    ipPacket[6] = 0x00; ipPacket[7] = 0x00; // Flags/Fragment
    ipPacket[8] = 64; // TTL
    ipPacket[9] = 6;  // Protocol TCP
    ipPacket[10] = 0x00; ipPacket[11] = 0x00; // Checksum (ignored)
    memcpy(ipPacket + 12, srcIp, 4);
    memcpy(ipPacket + 16, dstIp, 4);
    // Copy TCP + payload
    memcpy(ipPacket + 20, outBuf, outLen);
    
    // Decapsulate
    uint8_t decapBuf[2048];
    unsigned int decapLen = 0;
    uint16_t srcPort = 0, dstPort = 0;
    
    result = Udp2RawProtocol::decapsulateFakeTcp(
        ipPacket, 20 + outLen,
        decapBuf, decapLen,
        &srcPort, &dstPort
    );
    TEST_ASSERT(result);
    TEST_ASSERT_EQ(decapLen, ztLen);
    TEST_ASSERT(memcmp(decapBuf, ztData, ztLen) == 0);
    TEST_ASSERT_EQ(srcPort, 12345);
    TEST_ASSERT_EQ(dstPort, 443);
    
    printf("  PASSED\n");
    return 0;
}

// Test 4: ICMP encapsulation/decapsulation
int test_icmp_encap_decap()
{
    printf("Test: ICMP encapsulation/decapsulation...\n");
    
    // Test data
    const char* ztData = "Hello, ZeroTier over ICMP!";
    unsigned int ztLen = strlen(ztData) + 1;
    
    uint8_t outBuf[2048];
    unsigned int outLen = 0;
    
    // IPv4 addresses
    uint8_t srcIp[4] = {10, 0, 0, 1};
    uint8_t dstIp[4] = {10, 0, 0, 2};
    
    // Encapsulate
    bool result = Udp2RawProtocol::encapsulateIcmp(
        ztData, ztLen,
        outBuf, outLen,
        0x1234, 0x0001, // icmpId, icmpSeq
        srcIp, dstIp, false
    );
    TEST_ASSERT(result);
    TEST_ASSERT(outLen > 0);
    TEST_ASSERT(outLen >= Udp2RawProtocol::ICMP_HEADER_SIZE + Udp2RawProtocol::HEADER_SIZE + ztLen);
    
    // Build a fake IP packet
    uint8_t ipPacket[2048];
    ipPacket[0] = 0x45;
    ipPacket[1] = 0x00;
    uint16_t totalLen = htons(20 + outLen);
    memcpy(ipPacket + 2, &totalLen, 2);
    ipPacket[4] = 0x00; ipPacket[5] = 0x01;
    ipPacket[6] = 0x00; ipPacket[7] = 0x00;
    ipPacket[8] = 64;
    ipPacket[9] = 1;  // Protocol ICMP
    ipPacket[10] = 0x00; ipPacket[11] = 0x00;
    memcpy(ipPacket + 12, srcIp, 4);
    memcpy(ipPacket + 16, dstIp, 4);
    memcpy(ipPacket + 20, outBuf, outLen);
    
    // Decapsulate
    uint8_t decapBuf[2048];
    unsigned int decapLen = 0;
    uint16_t icmpId = 0, icmpSeq = 0;
    
    result = Udp2RawProtocol::decapsulateIcmp(
        ipPacket, 20 + outLen,
        decapBuf, decapLen,
        &icmpId, &icmpSeq
    );
    TEST_ASSERT(result);
    TEST_ASSERT_EQ(decapLen, ztLen);
    TEST_ASSERT(memcmp(decapBuf, ztData, ztLen) == 0);
    TEST_ASSERT_EQ(icmpId, 0x1234);
    TEST_ASSERT_EQ(icmpSeq, 0x0001);
    
    printf("  PASSED\n");
    return 0;
}

// Test 5: Handshake packet creation
int test_handshake()
{
    printf("Test: Handshake packet creation...\n");
    
    uint8_t buf[256];
    unsigned int len;
    
    // Create handshake request
    len = Udp2RawProtocol::createHandshake(buf, sizeof(buf), 
                                           Udp2RawProtocol::MODE_FAKE_TCP, 8080);
    TEST_ASSERT(len > 0);
    
    const Udp2RawProtocol::PacketHeader* hdr = 
        reinterpret_cast<const Udp2RawProtocol::PacketHeader*>(buf);
    TEST_ASSERT(Udp2RawProtocol::validateHeader(hdr, len));
    TEST_ASSERT_EQ(hdr->packetType, Udp2RawProtocol::PACKET_TYPE_HANDSHAKE);
    
    // Create handshake ACK
    len = Udp2RawProtocol::createHandshakeAck(buf, sizeof(buf),
                                             Udp2RawProtocol::MODE_FAKE_TCP, 8080);
    TEST_ASSERT(len > 0);
    hdr = reinterpret_cast<const Udp2RawProtocol::PacketHeader*>(buf);
    TEST_ASSERT(Udp2RawProtocol::validateHeader(hdr, len));
    TEST_ASSERT_EQ(hdr->packetType, Udp2RawProtocol::PACKET_TYPE_HANDSHAKE_ACK);
    
    printf("  PASSED\n");
    return 0;
}

// Test 6: Keepalive and probe packets
int test_control_packets()
{
    printf("Test: Keepalive and probe packets...\n");
    
    uint8_t buf[256];
    unsigned int len;
    
    // Create keepalive
    len = Udp2RawProtocol::createKeepalive(buf, sizeof(buf), 1000, 500);
    TEST_ASSERT(len > 0);
    
    const Udp2RawProtocol::PacketHeader* hdr = 
        reinterpret_cast<const Udp2RawProtocol::PacketHeader*>(buf);
    TEST_ASSERT(Udp2RawProtocol::validateHeader(hdr, len));
    TEST_ASSERT_EQ(hdr->packetType, Udp2RawProtocol::PACKET_TYPE_KEEPALIVE);
    TEST_ASSERT_EQ(hdr->seqNum, 1000);
    TEST_ASSERT_EQ(hdr->ackNum, 500);
    
    // Create probe
    len = Udp2RawProtocol::createProbe(buf, sizeof(buf));
    TEST_ASSERT(len > 0);
    hdr = reinterpret_cast<const Udp2RawProtocol::PacketHeader*>(buf);
    TEST_ASSERT(Udp2RawProtocol::validateHeader(hdr, len));
    TEST_ASSERT_EQ(hdr->packetType, Udp2RawProtocol::PACKET_TYPE_PROBE);
    
    // Test isProbePacket
    TEST_ASSERT(Udp2RawProtocol::isProbePacket(buf, len));
    TEST_ASSERT(!Udp2RawProtocol::isProbePacket(buf, len - 1)); // Too short
    
    // Test with wrong packet type
    Udp2RawProtocol::initHeader(reinterpret_cast<Udp2RawProtocol::PacketHeader*>(buf), 
                               Udp2RawProtocol::PACKET_TYPE_DATA, 0, 0, 0, 0);
    TEST_ASSERT(!Udp2RawProtocol::isProbePacket(buf, len));
    
    printf("  PASSED\n");
    return 0;
}

// Test 7: Edge cases
int test_edge_cases()
{
    printf("Test: Edge cases...\n");
    
    // Empty payload
    uint8_t outBuf[2048];
    unsigned int outLen = 0;
    uint8_t srcIp[4] = {192, 168, 1, 1};
    uint8_t dstIp[4] = {192, 168, 1, 2};
    
    bool result = Udp2RawProtocol::encapsulateFakeTcp(
        "", 0, outBuf, outLen, 12345, 443, 0, 0, 
        Udp2RawProtocol::TCP_FLAG_ACK, srcIp, dstIp, false
    );
    TEST_ASSERT(result);
    TEST_ASSERT_EQ(outLen, Udp2RawProtocol::TCP_HEADER_SIZE + Udp2RawProtocol::HEADER_SIZE);
    
    // Maximum payload
    uint8_t maxPayload[Udp2RawProtocol::MAX_PAYLOAD_SIZE];
    memset(maxPayload, 0xAA, sizeof(maxPayload));
    result = Udp2RawProtocol::encapsulateFakeTcp(
        maxPayload, sizeof(maxPayload), outBuf, outLen, 12345, 443, 0, 0,
        Udp2RawProtocol::TCP_FLAG_ACK, srcIp, dstIp, false
    );
    TEST_ASSERT(result);
    
    // Payload too large
    uint8_t largePayload[Udp2RawProtocol::MAX_PAYLOAD_SIZE + 1];
    result = Udp2RawProtocol::encapsulateFakeTcp(
        largePayload, sizeof(largePayload), outBuf, outLen, 12345, 443, 0, 0,
        Udp2RawProtocol::TCP_FLAG_ACK, srcIp, dstIp, false
    );
    TEST_ASSERT(!result);
    
    // Buffer too small
    uint8_t smallBuf[10];
    unsigned int smallLen = 0;
    result = Udp2RawProtocol::encapsulateFakeTcp(
        "test", 4, smallBuf, smallLen, 12345, 443, 0, 0,
        Udp2RawProtocol::TCP_FLAG_ACK, srcIp, dstIp, false
    );
    // Should fail or produce truncated output
    
    printf("  PASSED\n");
    return 0;
}

// Main test runner
int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    
    printf("======================================\n");
    printf("Udp2RawProtocol Unit Tests\n");
    printf("======================================\n\n");
    
    int failures = 0;
    
    failures += test_header_basic();
    failures += test_checksum();
    failures += test_faketcp_encap_decap();
    failures += test_icmp_encap_decap();
    failures += test_handshake();
    failures += test_control_packets();
    failures += test_edge_cases();
    
    printf("\n======================================\n");
    if (failures == 0) {
        printf("All tests PASSED!\n");
    } else {
        printf("%d test(s) FAILED!\n", failures);
    }
    printf("======================================\n");
    
    return failures;
}
