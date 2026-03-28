# ZeroTier Udp2Raw Integration

This document describes the udp2raw integration for ZeroTier, which allows bypassing UDP firewalls and QoS throttling by encapsulating UDP traffic in TCP or ICMP packets.

## Overview

Udp2raw is a protocol that disguises UDP traffic as TCP or ICMP, allowing it to bypass:
- Firewalls that block UDP
- ISP QoS throttling on UDP traffic
- Corporate network restrictions

## Features

- **Automatic detection**: Automatically detects if a peer supports udp2raw
- **Seamless fallback**: Falls back to standard UDP if udp2raw fails
- **Multiple modes**: Supports FakeTCP and ICMP encapsulation
- **Zero configuration**: Works out of the box with sensible defaults
- **Backward compatible**: Doesn't affect peers that don't support udp2raw

## Configuration

Add the following to your `local.conf`:

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

### Configuration Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `mode` | string | `"auto"` | Operating mode: `disabled`, `auto`, `preferred`, `required` |
| `transport` | string | `"tcp"` | Transport type: `tcp` (FakeTCP) or `icmp` |
| `localPort` | int | `0` | Local port (0 = auto) |
| `remotePort` | int | `443` | Remote port for FakeTCP |
| `icmpId` | int | `4660` | ICMP identifier |
| `probeInterval` | int | `30000` | Probe interval in ms |
| `handshakeTimeout` | int | `10000` | Handshake timeout in ms |
| `fallbackDelay` | int | `60000` | Delay before retry after failure |
| `autoUpgrade` | bool | `true` | Auto upgrade to udp2raw |
| `autoDowngrade` | bool | `true` | Auto fallback to UDP on failure |

### Modes

- **disabled**: Udp2raw is completely disabled
- **auto** (default): Automatically upgrade to udp2raw when both peers support it
- **preferred**: Prefer udp2raw over standard UDP
- **required**: Require udp2raw (connections will fail without it)

## Requirements

- Linux (Windows support planned)
- Root privileges or CAP_NET_RAW capability
- Both peers must support udp2raw for it to be used

## Building

The udp2raw integration is built automatically with ZeroTier:

```bash
make
sudo make install
```

## Testing

Run the unit tests:

```bash
# Protocol tests
cd node && make -f Udp2RawProtocolTest.mk test

# Socket tests
cd osdep && make -f Udp2RawSocketTest.mk test

# Path integration tests
cd node && make -f PathUdp2RawTest.mk test
```

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    ZeroTier Node                        │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐ │
│  │   Switch    │───▶│    Peer     │───▶│    Path     │ │
│  │             │    │             │    │             │ │
│  │             │    │  ┌───────┐  │    │  ┌────────┐ │ │
│  │             │    │  │Path   │  │    │  │Udp2Raw │ │ │
│  │             │    │  │Udp2Raw│  │    │  │Socket  │ │ │
│  └─────────────┘    │  └───────┘  │    │  └────────┘ │ │
│                     └─────────────┘    └─────────────┘ │
└─────────────────────────────────────────────────────────┘
                              │
                              ▼
                    ┌───────────────────┐
                    │   Raw Socket      │
                    │  (FakeTCP/ICMP)   │
                    └───────────────────┘
```

## Troubleshooting

### Udp2raw not working

1. Check if you have root privileges:
   ```bash
   sudo zerotier-cli status
   ```

2. Check if raw sockets are supported:
   ```bash
   sudo cat /proc/sys/net/ipv4/ping_group_range
   ```

3. Enable verbose logging in local.conf:
   ```json
   {"settings": {"udp2raw": {"logVerbose": true}}}
   ```

### Connection issues

1. Try different ports:
   ```json
   {"settings": {"udp2raw": {"remotePort": 80}}}
   ```

2. Try ICMP mode:
   ```json
   {"settings": {"udp2raw": {"transport": "icmp"}}}
   ```

## Security Considerations

- Udp2raw requires raw socket access (root or CAP_NET_RAW)
- FakeTCP packets are not real TCP connections
- All ZeroTier encryption is preserved
- No additional security risks compared to standard UDP

## License

This udp2raw integration follows the same license as ZeroTier (GPLv3).
