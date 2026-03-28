/*
 * ZeroTier One - Udp2RawSocket Unit Tests
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Udp2RawSocket.hpp"

using namespace ZeroTier;

#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s at line %d\n", #cond, __LINE__); \
        return 1; \
    } \
} while(0)

#define TEST_ASSERT_EQ(a, b) TEST_ASSERT((a) == (b))

int test_basic_functionality()
{
    printf("Test: Basic functionality...\n");
    
    // Test static methods
    printf("  Raw socket supported: %s\n", 
           Udp2RawSocket::isSupported() ? "yes" : "no");
    printf("  Has privileges: %s\n",
           Udp2RawSocket::hasPrivileges() ? "yes" : "no");
    
    // Test recommended ports
    unsigned int portCount = 0;
    const uint16_t* ports = Udp2RawSocket::getRecommendedPorts(portCount);
    TEST_ASSERT(ports != nullptr);
    TEST_ASSERT(portCount > 0);
    printf("  Recommended ports: %u\n", portCount);
    for (unsigned int i = 0; i < portCount && i < 5; ++i) {
        printf("    %u: %u\n", i, ports[i]);
    }
    
    // Test socket creation (will fail without privileges)
    Udp2RawSocket socket;
    TEST_ASSERT_EQ(socket.getState(), Udp2RawSocket::STATE_CLOSED);
    TEST_ASSERT(!socket.isOpen());
    
    Udp2RawSocket::Config config;
    config.mode = Udp2RawProtocol::MODE_FAKE_TCP;
    config.localPort = 12345;
    config.remotePort = 443;
    
    InetAddress localAddr; // Wildcard
    
    bool opened = socket.open(localAddr, config);
    if (Udp2RawSocket::hasPrivileges()) {
        TEST_ASSERT(opened);
        TEST_ASSERT(socket.isOpen());
        TEST_ASSERT_EQ(socket.getState(), Udp2RawSocket::STATE_OPEN);
        
        // Test sequence numbers
        socket.updateSeq(1000, 500);
        TEST_ASSERT_EQ(socket.getSeq(), 1000);
        TEST_ASSERT_EQ(socket.getAck(), 500);
        
        // Get stats
        Udp2RawSocket::Stats stats = socket.getStats();
        printf("  Stats: sent=%lu, recv=%lu\n",
               (unsigned long)stats.packetsSent,
               (unsigned long)stats.packetsReceived);
        
        socket.close();
        TEST_ASSERT(!socket.isOpen());
    } else {
        printf("  Skipping socket tests (no privileges)\n");
        TEST_ASSERT(!opened);
    }
    
    printf("  PASSED\n");
    return 0;
}

int test_config()
{
    printf("Test: Configuration...\n");
    
    Udp2RawSocket::Config config;
    
    // Test defaults
    TEST_ASSERT_EQ(config.mode, Udp2RawProtocol::MODE_FAKE_TCP);
    TEST_ASSERT_EQ(config.localPort, 0);
    TEST_ASSERT_EQ(config.remotePort, 443);
    TEST_ASSERT_EQ(config.icmpId, 0x1234);
    TEST_ASSERT(config.autoHandshake);
    TEST_ASSERT(config.autoKeepalive);
    TEST_ASSERT_EQ(config.keepaliveInterval, 30000);
    
    // Test custom config
    config.mode = Udp2RawProtocol::MODE_ICMP;
    config.localPort = 54321;
    config.remotePort = 80;
    config.icmpId = 0x5678;
    config.autoHandshake = false;
    
    TEST_ASSERT_EQ(config.mode, Udp2RawProtocol::MODE_ICMP);
    TEST_ASSERT_EQ(config.localPort, 54321);
    TEST_ASSERT_EQ(config.remotePort, 80);
    TEST_ASSERT_EQ(config.icmpId, 0x5678);
    TEST_ASSERT(!config.autoHandshake);
    
    printf("  PASSED\n");
    return 0;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    
    printf("======================================\n");
    printf("Udp2RawSocket Unit Tests\n");
    printf("======================================\n\n");
    
    int failures = 0;
    
    failures += test_config();
    failures += test_basic_functionality();
    
    printf("\n======================================\n");
    if (failures == 0) {
        printf("All tests PASSED!\n");
    } else {
        printf("%d test(s) FAILED!\n", failures);
    }
    printf("======================================\n");
    
    return failures;
}
