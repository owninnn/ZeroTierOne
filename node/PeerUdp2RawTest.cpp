/*
 * ZeroTier One - PeerUdp2Raw Unit Tests
 * Simplified test without full Path dependencies
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "PeerUdp2Raw.hpp"
#include "Udp2RawConfig.hpp"

using namespace ZeroTier;

#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s at line %d\n", #cond, __LINE__); \
        return 1; \
    } \
} while(0)

#define TEST_ASSERT_EQ(a, b) TEST_ASSERT((a) == (b))

int test_peer_udp2raw_basic()
{
    printf("Test: PeerUdp2Raw basic functionality...\n");
    
    PeerUdp2Raw peerUdp2Raw;
    
    // Initial state
    TEST_ASSERT(!peerUdp2Raw.isFailed());
    TEST_ASSERT_EQ(peerUdp2Raw.getLastFailureTime(), 0);
    TEST_ASSERT(peerUdp2Raw.canRetry(0));
    
    printf("  PASSED\n");
    return 0;
}

int test_failure_handling()
{
    printf("Test: Failure handling...\n");
    
    PeerUdp2Raw peerUdp2Raw;
    
    // Mark as failed
    peerUdp2Raw.markFailed(1000);
    TEST_ASSERT(peerUdp2Raw.isFailed());
    TEST_ASSERT_EQ(peerUdp2Raw.getLastFailureTime(), 1000);
    
    // Should not be able to retry immediately
    TEST_ASSERT(!peerUdp2Raw.canRetry(1000));
    TEST_ASSERT(!peerUdp2Raw.canRetry(60000)); // Before fallback delay
    
    // Should be able to retry after fallback delay (default 60000ms)
    TEST_ASSERT(peerUdp2Raw.canRetry(61001));
    
    // Reset failure
    peerUdp2Raw.resetFailure();
    TEST_ASSERT(!peerUdp2Raw.isFailed());
    TEST_ASSERT_EQ(peerUdp2Raw.getLastFailureTime(), 0);
    TEST_ASSERT(peerUdp2Raw.canRetry(0));
    
    printf("  PASSED\n");
    return 0;
}

int test_path_array_functions()
{
    printf("Test: Path array functions...\n");
    
    // Test with null paths (edge case)
    Path* paths[3] = {nullptr, nullptr, nullptr};
    
    bool hasUdp2Raw = PeerUdp2Raw::hasActiveUdp2RawPath(paths, 3);
    TEST_ASSERT(!hasUdp2Raw);
    
    unsigned int count = PeerUdp2Raw::getUdp2RawPathCount(paths, 3);
    TEST_ASSERT_EQ(count, 0);
    
    printf("  PASSED\n");
    return 0;
}

int test_global_config_integration()
{
    printf("Test: Global config integration...\n");
    
    // Check global config
    const Udp2RawConfig::Settings& settings = Udp2RawConfig::getInstance().getSettings();
    
    // Default should be auto mode
    TEST_ASSERT(Udp2RawConfig::getInstance().isEnabled());
    TEST_ASSERT(Udp2RawConfig::getInstance().autoUpgrade());
    
    printf("  Global mode: %s\n", Udp2RawConfig::getModeName(settings.mode));
    printf("  Auto upgrade: %s\n", settings.autoUpgrade ? "yes" : "no");
    printf("  Fallback delay: %u ms\n", settings.fallbackDelayMs);
    
    printf("  PASSED\n");
    return 0;
}

int test_config_values()
{
    printf("Test: Config values for PeerUdp2Raw...\n");
    
    const Udp2RawConfig::Settings& settings = Udp2RawConfig::getInstance().getSettings();
    
    // Verify values that PeerUdp2Raw uses
    TEST_ASSERT(settings.fallbackDelayMs > 0);
    TEST_ASSERT(settings.upgradeDelayMs > 0);
    TEST_ASSERT(settings.probeIntervalMs > 0);
    TEST_ASSERT(settings.handshakeTimeoutMs > 0);
    
    printf("  fallbackDelayMs: %u\n", settings.fallbackDelayMs);
    printf("  upgradeDelayMs: %u\n", settings.upgradeDelayMs);
    printf("  probeIntervalMs: %u\n", settings.probeIntervalMs);
    printf("  handshakeTimeoutMs: %u\n", settings.handshakeTimeoutMs);
    
    printf("  PASSED\n");
    return 0;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    
    printf("======================================\n");
    printf("PeerUdp2Raw Unit Tests\n");
    printf("======================================\n\n");
    
    int failures = 0;
    
    failures += test_peer_udp2raw_basic();
    failures += test_failure_handling();
    failures += test_path_array_functions();
    failures += test_global_config_integration();
    failures += test_config_values();
    
    printf("\n======================================\n");
    if (failures == 0) {
        printf("All tests PASSED!\n");
    } else {
        printf("%d test(s) FAILED!\n", failures);
    }
    printf("======================================\n");
    
    return failures;
}
