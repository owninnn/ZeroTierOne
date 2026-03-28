#!/bin/bash
#
# Udp2Raw Integration Test Script
# Tests the udp2raw integration components
#

set -e

echo "======================================"
echo "Udp2Raw Integration Test Suite"
echo "======================================"
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counter
TESTS_PASSED=0
TESTS_FAILED=0

# Function to run a test
run_test() {
    local test_name="$1"
    local test_cmd="$2"
    
    echo -n "Testing: $test_name ... "
    if eval "$test_cmd" > /dev/null 2>&1; then
        echo -e "${GREEN}PASSED${NC}"
        ((TESTS_PASSED++))
        return 0
    else
        echo -e "${RED}FAILED${NC}"
        ((TESTS_FAILED++))
        return 1
    fi
}

cd "$(dirname "$0")/node"

# Test 1: Protocol layer
echo "--- Protocol Layer Tests ---"
run_test "Udp2RawProtocol" "make -f Udp2RawProtocolTest.mk clean && make -f Udp2RawProtocolTest.mk && ./udp2raw_protocol_test"

# Test 2: Path layer
echo ""
echo "--- Path Layer Tests ---"
run_test "PathUdp2Raw" "make -f PathUdp2RawTest.mk clean && make -f PathUdp2RawTest.mk && ./path_udp2raw_test"

# Test 3: Integration tests
echo ""
echo "--- Integration Tests ---"
run_test "PathIntegration" "make -f PathIntegrationTest.mk clean && make -f PathIntegrationTest.mk && ./path_integration_test"

# Test 4: Peer layer (standalone)
echo ""
echo "--- Peer Layer Tests ---"
run_test "PeerUdp2RawStandalone" "make -f PeerUdp2RawStandaloneTest.mk clean && make -f PeerUdp2RawStandaloneTest.mk && ./peer_udp2raw_standalone_test"

# Test 5: Check file existence
echo ""
echo "--- File Existence Tests ---"
cd "$(dirname "$0")"
run_test "Path.hpp modified" "grep -q 'hasUdp2Raw' node/Path.hpp"
run_test "Path.cpp modified" "grep -q 'Udp2RawConfig' node/Path.cpp"
run_test "Udp2RawProtocol exists" "test -f node/Udp2RawProtocol.hpp"
run_test "Udp2RawSocket exists" "test -f osdep/Udp2RawSocket.hpp"
run_test "PathUdp2Raw exists" "test -f node/PathUdp2Raw.hpp"
run_test "PeerUdp2Raw exists" "test -f node/PeerUdp2Raw.hpp"
run_test "Udp2RawConfig exists" "test -f node/Udp2RawConfig.hpp"
run_test "Config example exists" "test -f udp2raw-local.conf.example"
run_test "README exists" "test -f UDP2RAW_README.md"

# Summary
echo ""
echo "======================================"
echo "Test Summary"
echo "======================================"
echo -e "Passed: ${GREEN}$TESTS_PASSED${NC}"
echo -e "Failed: ${RED}$TESTS_FAILED${NC}"
echo ""

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed!${NC}"
    exit 1
fi
