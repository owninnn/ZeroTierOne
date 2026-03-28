/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://www.mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

#include "Path.hpp"

#include "Node.hpp"
#include "RuntimeEnvironment.hpp"
#include "Udp2RawConfig.hpp"

namespace ZeroTier {

bool Path::send(const RuntimeEnvironment* RR, void* tPtr, const void* data, unsigned int len, int64_t now)
{
	// Try udp2raw first if available and active
	if (hasUdp2Raw()) {
		if (_udp2Raw->send(data, len)) {
			_lastOut = now;
			return true;
		}
		// Fall back to UDP on udp2raw failure if autoDowngrade is enabled
		if (!Udp2RawConfig::getInstance().autoDowngrade()) {
			return false;
		}
	}
	
	// Standard UDP send
	if (RR->node->putPacket(tPtr, _localSocket, _addr, data, len)) {
		_lastOut = now;
		return true;
	}
	return false;
}

bool Path::initUdp2Raw(const InetAddress& localAddr, const PathUdp2Raw::Config& config)
{
	if (!Udp2RawConfig::getInstance().isEnabled()) {
		return false;
	}
	
	if (_udp2Raw) {
		// Already initialized
		return true;
	}
	
	_udp2Raw = std::make_unique<PathUdp2Raw>();
	return _udp2Raw->init(localAddr, _addr, config);
}

bool Path::sendUdp2Raw(const void* data, unsigned int len)
{
	if (!hasUdp2Raw()) {
		return false;
	}
	return _udp2Raw->send(data, len);
}

bool Path::processUdp2RawPacket(const void* data, unsigned int len)
{
	if (!_udp2Raw) {
		return false;
	}
	return _udp2Raw->processIncomingPacket(data, len);
}

void Path::shutdownUdp2Raw()
{
	if (_udp2Raw) {
		_udp2Raw->shutdown();
		_udp2Raw.reset();
	}
}

} // namespace ZeroTier
