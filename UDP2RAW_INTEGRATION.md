# Udp2Raw Integration Guide

This document describes how to integrate udp2raw into ZeroTier's core components.

## Files to Modify

### 1. node/Path.hpp

Add to the Path class:

```cpp
#include "PathUdp2Raw.hpp"

// In class Path:
private:
    std::unique_ptr<PathUdp2Raw> _udp2Raw;
    
public:
    /**
     * Initialize udp2raw for this path
     */
    bool initUdp2Raw(const InetAddress& localAddr, const PathUdp2Raw::Config& config);
    
    /**
     * Check if udp2raw is active for this path
     */
    bool hasUdp2Raw() const { return _udp2Raw && _udp2Raw->isActive(); }
    
    /**
     * Send via udp2raw if available
     */
    bool sendUdp2Raw(const void* data, unsigned int len);
```

### 2. node/Path.cpp

Modify the `send()` method:

```cpp
bool Path::send(const RuntimeEnvironment* RR, void* tPtr, const void* data, unsigned int len, int64_t now)
{
    // Try udp2raw first if available
    if (hasUdp2Raw()) {
        if (_udp2Raw->send(data, len)) {
            _lastOut = now;
            return true;
        }
        // Fall back to UDP on udp2raw failure
    }
    
    // Standard UDP send
    if (RR->node->putPacket(tPtr, _localSocket, _addr, data, len)) {
        _lastOut = now;
        return true;
    }
    return false;
}
```

### 3. node/Peer.hpp

Add udp2raw initialization:

```cpp
#include "Udp2RawConfig.hpp"

// In class Peer:
private:
    void _tryInitUdp2Raw(const SharedPtr<Path>& path);
```

### 4. node/Peer.cpp

Modify `sendDirect()`:

```cpp
inline bool sendDirect(void* tPtr, const void* data, unsigned int len, int64_t now, bool force)
{
    SharedPtr<Path> bp(getAppropriatePath(now, force));
    if (bp) {
        // Try to init udp2raw if not already done
        if (!bp->hasUdp2Raw() && Udp2RawConfig::getInstance().autoUpgrade()) {
            _tryInitUdp2Raw(bp);
        }
        return bp->send(RR, tPtr, data, len, now);
    }
    return false;
}
```

### 5. service/OneService.cpp

Add configuration loading in `_loadLocalConfig()`:

```cpp
// After loading settings
json& udp2raw = settings["udp2raw"];
if (udp2raw.is_object()) {
    Udp2RawConfig::getInstance().loadFromJson(udp2raw);
}
```

## Build System Changes

Add to makefiles:

```makefile
# In node/objects.mk
OBJS += node/Udp2RawProtocol.o \
        node/Udp2RawConfig.o \
        node/PathUdp2Raw.o

# In osdep/objects.mk
OBJS += osdep/Udp2RawSocket.o
```

## Testing Integration

1. Build ZeroTier with udp2raw:
   ```bash
   make clean && make
   ```

2. Install:
   ```bash
   sudo make install
   ```

3. Configure:
   ```bash
   sudo tee /var/lib/zerotier-one/local.conf > /dev/null << 'EOF'
   {
       "settings": {
           "udp2raw": {
               "mode": "auto"
           }
       }
   }
   EOF
   ```

4. Restart:
   ```bash
   sudo systemctl restart zerotier-one
   ```

5. Check status:
   ```bash
   sudo zerotier-cli status
   ```

## Verification

Check if udp2raw is active:
- Look for "udp2raw" in logs: `sudo journalctl -u zerotier-one -f`
- Check peer paths: `sudo zerotier-cli peers`

## Notes

- This is a minimal integration guide
- Full integration requires careful testing
- Windows support requires additional work (WinDivert)
- Consider adding metrics and monitoring
