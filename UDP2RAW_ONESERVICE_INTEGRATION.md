# OneService Udp2Raw Integration Guide

This document describes how to integrate udp2raw configuration loading into OneService.

## Changes Required

### 1. Add includes to OneService.cpp

Add after line 19 (after `#include "OneService.hpp"`):

```cpp
#include "../node/Udp2RawConfig.hpp"
#include "../osdep/Udp2RawSocket.hpp"
```

### 2. Add configuration loading

Add after the OpenTelemetry configuration section (around line 1620):

```cpp
// Udp2Raw configuration
json& udp2raw = settings["udp2raw"];
if (udp2raw.is_object()) {
    // Load udp2raw configuration
    Udp2RawConfig::getInstance().loadFromJson(&udp2raw);
    
    if (Udp2RawConfig::getInstance().isEnabled()) {
        fprintf(stderr, "[udp2raw] Enabled (mode: %s)\n", 
            Udp2RawConfig::getModeName(Udp2RawConfig::getInstance().getSettings().mode));
        
        // Check if system supports udp2raw
        if (!Udp2RawSocket::isSupported()) {
            fprintf(stderr, "[udp2raw] WARNING: Raw sockets not supported on this platform\n");
        }
        if (!Udp2RawSocket::hasPrivileges()) {
            fprintf(stderr, "[udp2raw] WARNING: No privileges for raw sockets (run as root or with CAP_NET_RAW)\n");
        }
    }
}
```

### 3. Update makefiles

Add to `node/objects.mk`:

```makefile
OBJS += node/Udp2RawProtocol.o \
        node/Udp2RawConfig.o \
        node/PathUdp2Raw.o \
        node/PeerUdp2Raw.o
```

Add to `osdep/objects.mk`:

```makefile
OBJS += osdep/Udp2RawSocket.o
```

## Build Instructions

```bash
# Apply the patch
cd /path/to/zerotier-one
cp service/OneService.cpp service/OneService.cpp.bak
patch -p1 < service/OneService_udp2raw.patch

# Build
make clean
make

# Install
sudo make install
```

## Configuration

Create `/var/lib/zerotier-one/local.conf`:

```json
{
    "settings": {
        "udp2raw": {
            "mode": "auto",
            "transport": "tcp",
            "remotePort": 443
        }
    }
}
```

## Verification

1. Check startup logs:
   ```bash
   sudo journalctl -u zerotier-one -f
   ```
   Look for: `[udp2raw] Enabled (mode: auto)`

2. Check peer connections:
   ```bash
   sudo zerotier-cli peers
   ```

3. Test with a peer that also has udp2raw enabled

## Troubleshooting

### "Raw sockets not supported"
- Windows is not supported (requires WinDivert)
- Use Linux or macOS

### "No privileges for raw sockets"
- Run as root: `sudo zerotier-one`
- Or add capability: `sudo setcap cap_net_raw+ep /usr/sbin/zerotier-one`

### Udp2raw not activating
- Check both peers have udp2raw enabled
- Check firewall rules allow the chosen port
- Enable verbose logging: `"logVerbose": true`
