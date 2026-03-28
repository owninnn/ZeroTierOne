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

#include "PeerUdp2Raw.hpp"

namespace ZeroTier {

PeerUdp2Raw::PeerUdp2Raw() :
    _failed(false),
    _lastFailureTime(0),
    _lastUpgradeAttempt(0),
    _upgradeAttemptCount(0)
{
}

PeerUdp2Raw::~PeerUdp2Raw()
{
}

bool PeerUdp2Raw::tryInitPath(Path& path, const InetAddress& localAddr)
{
    // Check if udp2raw is globally enabled
    if (!Udp2RawConfig::getInstance().isEnabled()) {
        return false;
    }
    
    // Check if already has udp2raw
    if (path.hasUdp2Raw()) {
        return true;
    }
    
    // Check if failed and can't retry yet
    if (_failed.load() && !canRetry(0)) {
        return false;
    }
    
    // Create config from global settings
    PathUdp2Raw::Config config;
    const Udp2RawConfig::Settings& settings = Udp2RawConfig::getInstance().getSettings();
    
    config.enabled = true;
    config.preferredMode = settings.transportMode;
    config.localPort = settings.localPort;
    config.remotePort = settings.remotePort;
    config.icmpId = settings.icmpId;
    config.probeIntervalMs = settings.probeIntervalMs;
    config.handshakeTimeoutMs = settings.handshakeTimeoutMs;
    config.fallbackDelayMs = settings.fallbackDelayMs;
    config.autoUpgrade = settings.autoUpgrade;
    config.autoDowngrade = settings.autoDowngrade;
    
    // Try to initialize
    bool result = path.initUdp2Raw(localAddr, config);
    
    if (result) {
        _upgradeAttemptCount.fetch_add(1);
        _lastUpgradeAttempt.store(0); // Will be set by caller with actual time
    }
    
    return result;
}

bool PeerUdp2Raw::shouldTryUpgrade(const Path& path, int64_t now) const
{
    // Check global settings
    if (!Udp2RawConfig::getInstance().autoUpgrade()) {
        return false;
    }
    
    // Check if path is valid and alive
    if (!path.valid() || !path.alive(now)) {
        return false;
    }
    
    // Check if already has udp2raw
    if (path.hasUdp2Raw()) {
        return false;
    }
    
    // Check if failed and can't retry
    if (_failed.load() && !canRetry(now)) {
        return false;
    }
    
    // Check upgrade delay
    const Udp2RawConfig::Settings& settings = Udp2RawConfig::getInstance().getSettings();
    int64_t lastAttempt = _lastUpgradeAttempt.load();
    if (lastAttempt > 0 && (now - lastAttempt) < settings.upgradeDelayMs) {
        return false;
    }
    
    return true;
}

void PeerUdp2Raw::markFailed(int64_t now)
{
    _failed.store(true);
    _lastFailureTime.store(now);
}

bool PeerUdp2Raw::canRetry(int64_t now) const
{
    if (!_failed.load()) {
        return true;
    }
    
    const Udp2RawConfig::Settings& settings = Udp2RawConfig::getInstance().getSettings();
    int64_t lastFailure = _lastFailureTime.load();
    
    if (lastFailure == 0) {
        return true;
    }
    
    return (now - lastFailure) > settings.fallbackDelayMs;
}

void PeerUdp2Raw::resetFailure()
{
    _failed.store(false);
    _lastFailureTime.store(0);
    _upgradeAttemptCount.store(0);
}

bool PeerUdp2Raw::hasActiveUdp2RawPath(Path* const* paths, unsigned int pathCount)
{
    for (unsigned int i = 0; i < pathCount; ++i) {
        if (paths[i] && paths[i]->hasUdp2Raw()) {
            return true;
        }
    }
    return false;
}

unsigned int PeerUdp2Raw::getUdp2RawPathCount(Path* const* paths, unsigned int pathCount)
{
    unsigned int count = 0;
    for (unsigned int i = 0; i < pathCount; ++i) {
        if (paths[i] && paths[i]->hasUdp2Raw()) {
            ++count;
        }
    }
    return count;
}

} // namespace ZeroTier
