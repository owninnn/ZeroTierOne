/*
 * ZeroTier One - Path Integration Test with Udp2Raw
 * Simplified test without full ZeroTier dependencies
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Udp2RawConfig.hpp"
#include "PathUdp2Raw.hpp"

using namespace ZeroTier;

#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s at line %d\n", #cond, __LINE__); \
        return 1; \
    } \
} while(0)

#define TEST_ASSERT_EQ(a, b) TEST_ASSERT((a) == (b))

int test_config_modes()
{
    printf("Test: Udp2RawConfig modes...\n");
    
    // Test mode parsing
    Udp2RawConfig::GlobalMode mode;
    
    mode = Udp2RawConfig::parseMode("disabled");
    TEST_ASSERT_EQ(mode, Udp2RawConfig::MODE_DISABLED);
    
    mode = Udp2RawConfig::parseMode("auto");
    TEST_ASSERT_EQ(mode, Udp2RawConfig::MODE_AUTO);
    
    mode = Udp2RawConfig::parseMode("preferred");
    TEST_ASSERT_EQ(mode, Udp2RawConfig::MODE_PREFERRED);
    
    mode = Udp2RawConfig::parseMode("required");
    TEST_ASSERT_EQ(mode, Udp2RawConfig::MODE_REQUIRED);
    
    mode = Udp2RawConfig::parseMode("off");
    TEST_ASSERT_EQ(mode, Udp2RawConfig::MODE_DISABLED);
    
    mode = Udp2RawConfig::parseMode("force");
    TEST_ASSERT_EQ(mode, Udp2RawConfig::MODE_REQUIRED);
    
    mode = Udp2RawConfig::parseMode("invalid");
    TEST_ASSERT_EQ(mode, Udp2RawConfig::MODE_AUTO); // Default
    
    mode = Udp2RawConfig::parseMode(nullptr);
    TEST_ASSERT_EQ(mode, Udp2RawConfig::MODE_AUTO); // Default
    
    // Test mode names
    const char* name = Udp2RawConfig::getModeName(Udp2RawConfig::MODE_DISABLED);
    TEST_ASSERT(strcmp(name, "disabled") == 0);
    
    name = Udp2RawConfig::getModeName(Udp2RawConfig::MODE_AUTO);
    TEST_ASSERT(strcmp(name, "auto") == 0);
    
    name = Udp2RawConfig::getModeName(Udp2RawConfig::MODE_PREFERRED);
    TEST_ASSERT(strcmp(name, "preferred") == 0);
    
    name = Udp2RawConfig::getModeName(Udp2RawConfig::MODE_REQUIRED);
    TEST_ASSERT(strcmp(name, "required") == 0);
    
    printf("  PASSED\n");
    return 0;
}

int test_transport_modes()
{
    printf("Test: Transport mode parsing...\n");
    
    Udp2RawProtocol::TransportMode tmode;
    
    tmode = Udp2RawConfig::parseTransportMode("tcp");
    TEST_ASSERT_EQ(tmode, Udp2RawProtocol::MODE_FAKE_TCP);
    
    tmode = Udp2RawConfig::parseTransportMode("faketcp");
    TEST_ASSERT_EQ(tmode, Udp2RawProtocol::MODE_FAKE_TCP);
    
    tmode = Udp2RawConfig::parseTransportMode("icmp");
    TEST_ASSERT_EQ(tmode, Udp2RawProtocol::MODE_ICMP);
    
    tmode = Udp2RawConfig::parseTransportMode("auto");
    TEST_ASSERT_EQ(tmode, Udp2RawProtocol::MODE_FAKE_TCP); // Default
    
    tmode = Udp2RawConfig::parseTransportMode("invalid");
    TEST_ASSERT_EQ(tmode, Udp2RawProtocol::MODE_FAKE_TCP); // Default
    
    printf("  PASSED\n");
    return 0;
}

int test_config_defaults()
{
    printf("Test: Udp2RawConfig defaults...\n");
    
    const Udp2RawConfig::Settings& settings = Udp2RawConfig::getInstance().getSettings();
    
    // Test default values
    TEST_ASSERT_EQ(settings.mode, Udp2RawConfig::MODE_AUTO);
    TEST_ASSERT_EQ(settings.transportMode, Udp2RawProtocol::MODE_FAKE_TCP);
    TEST_ASSERT_EQ(settings.localPort, 0);
    TEST_ASSERT_EQ(settings.remotePort, 443);
    TEST_ASSERT_EQ(settings.icmpId, 0x1234);
    TEST_ASSERT_EQ(settings.probeIntervalMs, 30000);
    TEST_ASSERT_EQ(settings.handshakeTimeoutMs, 10000);
    TEST_ASSERT_EQ(settings.fallbackDelayMs, 60000);
    TEST_ASSERT_EQ(settings.upgradeDelayMs, 5000);
    TEST_ASSERT(settings.autoUpgrade);
    TEST_ASSERT(settings.autoDowngrade);
    TEST_ASSERT(!settings.logVerbose);
    
    printf("  Default mode: %s\n", Udp2RawConfig::getModeName(settings.mode));
    printf("  Default transport: %s\n", 
           settings.transportMode == Udp2RawProtocol::MODE_FAKE_TCP ? "tcp" : "icmp");
    printf("  Default remotePort: %u\n", settings.remotePort);
    
    printf("  PASSED\n");
    return 0;
}

int test_config_checks()
{
    printf("Test: Udp2RawConfig checks...\n");
    
    // Default should be enabled (auto mode)
    TEST_ASSERT(Udp2RawConfig::getInstance().isEnabled());
    TEST_ASSERT(!Udp2RawConfig::getInstance().isRequired());
    TEST_ASSERT(Udp2RawConfig::getInstance().autoUpgrade());
    TEST_ASSERT(Udp2RawConfig::getInstance().autoDowngrade());
    
    printf("  PASSED\n");
    return 0;
}

int test_default_config_snippet()
{
    printf("Test: Default config snippet...\n");
    
    const char* snippet = Udp2RawConfig::getDefaultConfigSnippet();
    TEST_ASSERT(snippet != nullptr);
    TEST_ASSERT(strlen(snippet) > 0);
    
    // Check that it contains expected fields
    TEST_ASSERT(strstr(snippet, "udp2raw") != nullptr);
    TEST_ASSERT(strstr(snippet, "mode") != nullptr);
    TEST_ASSERT(strstr(snippet, "transport") != nullptr);
    
    printf("  Config snippet present: yes\n");
    
    printf("  PASSED\n");
    return 0;
}

int test_path_udp2raw_lifecycle()
{
    printf("Test: PathUdp2Raw lifecycle...\n");
    
    PathUdp2Raw pathUdp2Raw;
    
    // Initial state
    TEST_ASSERT_EQ(pathUdp2Raw.getState(), PathUdp2Raw::UDP2RAW_DISABLED);
    TEST_ASSERT_EQ(pathUdp2Raw.getTransportType(), PathUdp2Raw::TRANSPORT_UDP);
    TEST_ASSERT(!pathUdp2Raw.isActive());
    TEST_ASSERT(!pathUdp2Raw.shouldUseUdp2Raw());
    
    // Try to init (will fail without privileges)
    PathUdp2Raw::Config config;
    config.enabled = true;
    
    InetAddress localAddr;
    InetAddress remoteAddr;
    
    bool result = pathUdp2Raw.init(localAddr, remoteAddr, config);
    
    if (PathUdp2Raw::isSystemSupported()) {
        // Should succeed with privileges
        if (result) {
            TEST_ASSERT(pathUdp2Raw.getState() == PathUdp2Raw::UDP2RAW_PROBING ||
                       pathUdp2Raw.getState() == PathUdp2Raw::UDP2RAW_FAILED);
        }
    }
    
    // Shutdown
    pathUdp2Raw.shutdown();
    TEST_ASSERT_EQ(pathUdp2Raw.getState(), PathUdp2Raw::UDP2RAW_DISABLED);
    
    printf("  PASSED\n");
    return 0;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    
    printf("======================================\n");
    printf("Path Integration Test with Udp2Raw\n");
    printf("======================================\n\n");
    
    int failures = 0;
    
    failures += test_config_modes();
    failures += test_transport_modes();
    failures += test_config_defaults();
    failures += test_config_checks();
    failures += test_default_config_snippet();
    failures += test_path_udp2raw_lifecycle();
    
    printf("\n======================================\n");
    if (failures == 0) {
        printf("All tests PASSED!\n");
    } else {
        printf("%d test(s) FAILED!\n", failures);
    }
    printf("======================================\n");
    
    return failures;
}
