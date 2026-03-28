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
 * Peer-level udp2raw management
 */

#ifndef ZT_PEER_UDP2RAW_HPP
#define ZT_PEER_UDP2RAW_HPP

#include "Path.hpp"
#include "PathUdp2Raw.hpp"
#include "Udp2RawConfig.hpp"

#include <atomic>
#include <cstdint>

namespace ZeroTier {

/**
 * Manages udp2raw for a peer
 * 
 * This class handles:
 * - Auto-upgrade paths to udp2raw
 * - Path udp2raw initialization
 * - Fallback management
 */
class PeerUdp2Raw {
public:
    /**
     * Constructor
     */
    PeerUdp2Raw();
    
    /**
     * Destructor
     */
    ~PeerUdp2Raw();
    
    /**
     * Try to initialize udp2raw for a path
     * @param path Path to initialize
     * @param localAddr Local address
     * @return true if udp2raw was initialized or already active
     */
    bool tryInitPath(Path& path, const InetAddress& localAddr);
    
    /**
     * Check if we should try to upgrade this path to udp2raw
     * @param path Path to check
     * @param now Current time
     * @return true if should try to upgrade
     */
    bool shouldTryUpgrade(const Path& path, int64_t now) const;
    
    /**
     * Mark udp2raw as failed for this peer
     * Will prevent retry for fallbackDelayMs
     */
    void markFailed(int64_t now);
    
    /**
     * Check if udp2raw is currently in failed state
     */
    bool isFailed() const { return _failed.load(); }
    
    /**
     * Check if we can retry udp2raw after failure
     * @param now Current time
     * @return true if can retry
     */
    bool canRetry(int64_t now) const;
    
    /**
     * Get last failure time
     */
    int64_t getLastFailureTime() const { return _lastFailureTime.load(); }
    
    /**
     * Reset failure state (e.g., after successful connection)
     */
    void resetFailure();
    
    /**
     * Check if peer has any active udp2raw paths
     * @param paths Array of paths to check
     * @param pathCount Number of paths
     * @return true if any path has active udp2raw
     */
    static bool hasActiveUdp2RawPath(Path* const* paths, unsigned int pathCount);
    
    /**
     * Get count of paths with udp2raw
     * @param paths Array of paths to check
     * @param pathCount Number of paths
     * @return Number of paths with udp2raw
     */
    static unsigned int getUdp2RawPathCount(Path* const* paths, unsigned int pathCount);

private:
    std::atomic<bool> _failed;
    std::atomic<int64_t> _lastFailureTime;
    std::atomic<int64_t> _lastUpgradeAttempt;
    std::atomic<unsigned int> _upgradeAttemptCount;
};

} // namespace ZeroTier

#endif // ZT_PEER_UDP2RAW_HPP
