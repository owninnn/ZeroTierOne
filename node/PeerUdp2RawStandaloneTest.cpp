/*
 * ZeroTier One - PeerUdp2Raw Standalone Tests
 * Tests PeerUdp2Raw without full ZeroTier dependencies
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Udp2RawConfig.hpp"

using namespace ZeroTier;

#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s at line %d\n", #cond, __LINE__); \
        return 1; \
    } \
} while(0)

#define TEST_ASSERT_EQ(a, b) TEST_ASSERT((a) == (b))

// Minimal PeerUdp2Raw implementation for testing
class TestPeerUdp2Raw {
public:
    TestPeerUdp2Raw() : _failed(false), _lastFailureTime(0), _lastUpgradeAttempt(0) {}
    
    void markFailed(int64_t now) {
        _failed = true;
        _lastFailureTime = now;
    }
    
    void resetFailure() {
        _failed = false;
        _lastFailureTime = 0;
    }
    
    bool isFailed() const { return _failed; }
    int64_t getLastFailureTime() const { return _lastFailureTime; }
    
    bool canRetry(int64_t now) const {
        if (!_failed) return true;
        const auto& settings = Udp2RawConfig::getInstance().getSettings();
        if (_lastFailureTime == 0) return true;
        return (now - _lastFailureTime) > settings.fallbackDelayMs;
    }
    
private:
    bool _failed;
    int64_t _lastFailureTime;
    int64_t _lastUpgradeAttempt;
};

int test_failure_handling()
{
    printf("Test: Failure handling...\n");
    
    TestPeerUdp2Raw peerUdp2Raw;
    
    // Initial state
    TEST_ASSERT(!peerUdp2Raw.isFailed());
    TEST_ASSERT_EQ(peerUdp2Raw.getLastFailureTime(), 0);
    TEST_ASSERT(peerUdp2Raw.canRetry(0));
    
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

int test_global_config()
{
    printf("Test: Global config...\n");
    
    const Udp2RawConfig::Settings& settings = Udp2RawConfig::getInstance().getSettings();
    
    // Verify default values
    TEST_ASSERT_EQ(settings.mode, Udp2RawConfig::MODE_AUTO);
    TEST_ASSERT(settings.autoUpgrade);
    TEST_ASSERT(settings.autoDowngrade);
    TEST_ASSERT_EQ(settings.fallbackDelayMs, 60000);
    TEST_ASSERT_EQ(settings.upgradeDelayMs, 5000);
    
    printf("  Mode: %s\n", Udp2RawConfig::getModeName(settings.mode));
    printf("  Fallback delay: %u ms\n", settings.fallbackDelayMs);
    printf("  Upgrade delay: %u ms\n", settings.upgradeDelayMs);
    
    printf("  PASSED\n");
    return 0;
}

int test_mode_parsing()
{
    printf("Test: Mode parsing...\n");
    
    TEST_ASSERT_EQ(Udp2RawConfig::parseMode("disabled"), Udp2RawConfig::MODE_DISABLED);
    TEST_ASSERT_EQ(Udp2RawConfig::parseMode("auto"), Udp2RawConfig::MODE_AUTO);
    TEST_ASSERT_EQ(Udp2RawConfig::parseMode("preferred"), Udp2RawConfig::MODE_PREFERRED);
    TEST_ASSERT_EQ(Udp2RawConfig::parseMode("required"), Udp2RawConfig::MODE_REQUIRED);
    TEST_ASSERT_EQ(Udp2RawConfig::parseMode("off"), Udp2RawConfig::MODE_DISABLED);
    TEST_ASSERT_EQ(Udp2RawConfig::parseMode("force"), Udp2RawConfig::MODE_REQUIRED);
    TEST_ASSERT_EQ(Udp2RawConfig::parseMode("invalid"), Udp2RawConfig::MODE_AUTO);
    
    printf("  PASSED\n");
    return 0;
}

int test_transport_parsing()
{
    printf("Test: Transport mode parsing...\n");
    
    TEST_ASSERT_EQ(Udp2RawConfig::parseTransportMode("tcp"), Udp2RawProtocol::MODE_FAKE_TCP);
    TEST_ASSERT_EQ(Udp2RawConfig::parseTransportMode("faketcp"), Udp2RawProtocol::MODE_FAKE_TCP);
    TEST_ASSERT_EQ(Udp2RawConfig::parseTransportMode("icmp"), Udp2RawProtocol::MODE_ICMP);
    TEST_ASSERT_EQ(Udp2RawConfig::parseTransportMode("auto"), Udp2RawProtocol::MODE_FAKE_TCP);
    
    printf("  PASSED\n");
    return 0;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    
    printf("======================================\n");
    printf("PeerUdp2Raw Standalone Tests\n");
    printf("======================================\n\n");
    
    int failures = 0;
    
    failures += test_failure_handling();
    failures += test_global_config();
    failures += test_mode_parsing();
    failures += test_transport_parsing();
    
    printf("\n======================================\n");
    if (failures == 0) {
        printf("All tests PASSED!\n");
    } else {
        printf("%d test(s) FAILED!\n", failures);
    }
    printf("======================================\n");
    
    return failures;
}
