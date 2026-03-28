/*
 * ZeroTier One - PathUdp2Raw Unit Tests
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "PathUdp2Raw.hpp"

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
    printf("Test: Basic PathUdp2Raw functionality...\n");
    
    PathUdp2Raw pathUdp2Raw;
    
    // Test initial state
    TEST_ASSERT_EQ(pathUdp2Raw.getState(), PathUdp2Raw::UDP2RAW_DISABLED);
    TEST_ASSERT_EQ(pathUdp2Raw.getTransportType(), PathUdp2Raw::TRANSPORT_UDP);
    TEST_ASSERT(!pathUdp2Raw.isActive());
    TEST_ASSERT(!pathUdp2Raw.shouldUseUdp2Raw());
    
    // Test system support
    bool supported = PathUdp2Raw::isSystemSupported();
    printf("  System supported: %s\n", supported ? "yes" : "no");
    
    // Test name functions
    const char* typeName = PathUdp2Raw::getTransportTypeName(PathUdp2Raw::TRANSPORT_UDP);
    TEST_ASSERT(strcmp(typeName, "UDP") == 0);
    
    typeName = PathUdp2Raw::getTransportTypeName(PathUdp2Raw::TRANSPORT_UDP2RAW_TCP);
    TEST_ASSERT(strcmp(typeName, "UDP2RAW_TCP") == 0);
    
    typeName = PathUdp2Raw::getTransportTypeName(PathUdp2Raw::TRANSPORT_UDP2RAW_ICMP);
    TEST_ASSERT(strcmp(typeName, "UDP2RAW_ICMP") == 0);
    
    const char* stateName = PathUdp2Raw::getStateName(PathUdp2Raw::UDP2RAW_DISABLED);
    TEST_ASSERT(strcmp(stateName, "DISABLED") == 0);
    
    stateName = PathUdp2Raw::getStateName(PathUdp2Raw::UDP2RAW_ESTABLISHED);
    TEST_ASSERT(strcmp(stateName, "ESTABLISHED") == 0);
    
    printf("  PASSED\n");
    return 0;
}

int test_config()
{
    printf("Test: PathUdp2Raw configuration...\n");
    
    PathUdp2Raw::Config config;
    
    // Test defaults
    TEST_ASSERT(config.enabled);
    TEST_ASSERT_EQ(config.preferredMode, Udp2RawProtocol::MODE_FAKE_TCP);
    TEST_ASSERT_EQ(config.localPort, 0);
    TEST_ASSERT_EQ(config.remotePort, 443);
    TEST_ASSERT_EQ(config.icmpId, 0x1234);
    TEST_ASSERT_EQ(config.probeIntervalMs, 30000);
    TEST_ASSERT_EQ(config.handshakeTimeoutMs, 10000);
    TEST_ASSERT(config.autoUpgrade);
    TEST_ASSERT(config.autoDowngrade);
    
    // Test custom config
    config.enabled = false;
    config.preferredMode = Udp2RawProtocol::MODE_ICMP;
    config.remotePort = 80;
    config.autoUpgrade = false;
    
    TEST_ASSERT(!config.enabled);
    TEST_ASSERT_EQ(config.preferredMode, Udp2RawProtocol::MODE_ICMP);
    TEST_ASSERT_EQ(config.remotePort, 80);
    TEST_ASSERT(!config.autoUpgrade);
    
    printf("  PASSED\n");
    return 0;
}

int test_state_transitions()
{
    printf("Test: State transitions...\n");
    
    PathUdp2Raw pathUdp2Raw;
    
    // Create addresses
    InetAddress localAddr;
    InetAddress remoteAddr;
    
    // Test init with disabled config
    PathUdp2Raw::Config config;
    config.enabled = false;
    
    bool result = pathUdp2Raw.init(localAddr, remoteAddr, config);
    TEST_ASSERT(result);
    TEST_ASSERT_EQ(pathUdp2Raw.getState(), PathUdp2Raw::UDP2RAW_DISABLED);
    
    // Test init with enabled config (but may fail without privileges)
    PathUdp2Raw pathUdp2Raw2;
    config.enabled = true;
    
    result = pathUdp2Raw2.init(localAddr, remoteAddr, config);
    if (PathUdp2Raw::isSystemSupported()) {
        TEST_ASSERT(result);
        // Should be in PROBING state
        TEST_ASSERT_EQ(pathUdp2Raw2.getState(), PathUdp2Raw::UDP2RAW_PROBING);
    } else {
        // Should fail if no privileges
        TEST_ASSERT(!result || pathUdp2Raw2.getState() == PathUdp2Raw::UDP2RAW_FAILED);
    }
    
    // Test shutdown
    pathUdp2Raw2.shutdown();
    TEST_ASSERT_EQ(pathUdp2Raw2.getState(), PathUdp2Raw::UDP2RAW_DISABLED);
    
    printf("  PASSED\n");
    return 0;
}

int test_stats()
{
    printf("Test: Statistics...\n");
    
    PathUdp2Raw pathUdp2Raw;
    
    uint64_t sent, received, errors;
    pathUdp2Raw.getStats(sent, received, errors);
    
    TEST_ASSERT_EQ(sent, 0);
    TEST_ASSERT_EQ(received, 0);
    TEST_ASSERT_EQ(errors, 0);
    
    printf("  PASSED\n");
    return 0;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    
    printf("======================================\n");
    printf("PathUdp2Raw Unit Tests\n");
    printf("======================================\n\n");
    
    int failures = 0;
    
    failures += test_basic_functionality();
    failures += test_config();
    failures += test_state_transitions();
    failures += test_stats();
    
    printf("\n======================================\n");
    if (failures == 0) {
        printf("All tests PASSED!\n");
    } else {
        printf("%d test(s) FAILED!\n", failures);
    }
    printf("======================================\n");
    
    return failures;
}
