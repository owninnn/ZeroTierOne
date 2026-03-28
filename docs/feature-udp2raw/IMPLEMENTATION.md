# Udp2Raw 实现细节

## 文件清单

### 核心实现文件

| 文件 | 行数 | 说明 |
|------|------|------|
| `node/Udp2RawProtocol.hpp` | ~210 | 协议定义 |
| `node/Udp2RawProtocol.cpp` | ~420 | 协议实现 |
| `osdep/Udp2RawSocket.hpp` | ~310 | Socket 定义 |
| `osdep/Udp2RawSocket.cpp` | ~500 | Socket 实现 |
| `node/PathUdp2Raw.hpp` | ~250 | Path 扩展定义 |
| `node/PathUdp2Raw.cpp` | ~350 | Path 扩展实现 |
| `node/PeerUdp2Raw.hpp` | ~110 | Peer 扩展定义 |
| `node/PeerUdp2Raw.cpp` | ~150 | Peer 扩展实现 |
| `node/Udp2RawConfig.hpp` | ~160 | 配置定义 |
| `node/Udp2RawConfig.cpp` | ~140 | 配置实现 |

### 修改的文件

| 文件 | 修改内容 |
|------|---------|
| `node/Path.hpp` | 添加 udp2raw 成员和方法 |
| `node/Path.cpp` | 实现 udp2raw 发送逻辑 |

### 测试文件

| 文件 | 说明 |
|------|------|
| `node/Udp2RawProtocolTest.cpp` | 协议层单元测试 |
| `node/PathUdp2RawTest.cpp` | Path 层单元测试 |
| `node/PathIntegrationTest.cpp` | 集成测试 |
| `node/PeerUdp2RawStandaloneTest.cpp` | Peer 层测试 |
| `osdep/Udp2RawSocketTest.cpp` | Socket 层测试 |

## 关键算法

### TCP 校验和计算

```cpp
uint16_t Udp2RawProtocol::calculateTcpChecksum(
    const void* tcpHeader, 
    unsigned int tcpLen,
    const uint8_t* srcIp, 
    const uint8_t* dstIp, 
    bool isIPv6
) {
    uint32_t sum = 0;
    const uint16_t* data = static_cast<const uint16_t*>(tcpHeader);
    
    // Sum TCP header and data
    for (unsigned int i = 0; i < tcpLen / 2; ++i) {
        sum += data[i];
    }
    if (tcpLen % 2) {
        sum += static_cast<const uint8_t*>(tcpHeader)[tcpLen - 1] << 8;
    }
    
    // Add pseudo-header
    if (isIPv6) {
        // IPv6: src (16) + dst (16) + length (4) + next header (4)
        for (int i = 0; i < 8; ++i) {
            sum += reinterpret_cast<const uint16_t*>(srcIp)[i];
        }
        for (int i = 0; i < 8; ++i) {
            sum += reinterpret_cast<const uint16_t*>(dstIp)[i];
        }
        sum += htons(static_cast<uint16_t>(tcpLen));
        sum += htons(static_cast<uint16_t>(tcpLen >> 16));
        sum += htons(6); // TCP protocol
    } else {
        // IPv4: src (4) + dst (4) + zero (1) + protocol (1) + length (2)
        sum += reinterpret_cast<const uint16_t*>(srcIp)[0];
        sum += reinterpret_cast<const uint16_t*>(srcIp)[1];
        sum += reinterpret_cast<const uint16_t*>(dstIp)[0];
        sum += reinterpret_cast<const uint16_t*>(dstIp)[1];
        sum += htons(6);
        sum += htons(static_cast<uint16_t>(tcpLen));
    }
    
    // Fold 32-bit sum to 16-bit
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return static_cast<uint16_t>(~sum);
}
```

### FakeTCP 封装

```cpp
bool Udp2RawProtocol::encapsulateFakeTcp(
    const void* ztData, unsigned int ztLen,
    void* outBuf, unsigned int& outLen,
    uint16_t srcPort, uint16_t dstPort,
    uint32_t seq, uint32_t ack, uint8_t flags,
    const uint8_t* srcIp, const uint8_t* dstIp, bool isIPv6
) {
    uint8_t* buf = static_cast<uint8_t*>(outBuf);
    unsigned int offset = 0;
    
    // TCP header
    TcpHeader* tcp = reinterpret_cast<TcpHeader*>(buf + offset);
    tcp->sourcePort = htons(srcPort);
    tcp->destPort = htons(dstPort);
    tcp->seqNum = htonl(seq);
    tcp->ackNum = htonl(ack);
    tcp->dataOffset = (5 << 4); // 20 bytes, no options
    tcp->flags = flags;
    tcp->windowSize = htons(65535);
    tcp->urgentPtr = 0;
    offset += TCP_HEADER_SIZE;
    
    // Udp2Raw header
    PacketHeader* hdr = reinterpret_cast<PacketHeader*>(buf + offset);
    initHeader(hdr, PACKET_TYPE_DATA, static_cast<uint16_t>(ztLen), seq, ack, 0);
    offset += HEADER_SIZE;
    
    // Payload
    memcpy(buf + offset, ztData, ztLen);
    offset += ztLen;
    
    // Calculate checksum
    tcp->checksum = 0;
    tcp->checksum = calculateTcpChecksum(tcp, offset, srcIp, dstIp, isIPv6);
    
    outLen = offset;
    return true;
}
```

### 路径升级决策

```cpp
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
    const auto& settings = Udp2RawConfig::getInstance().getSettings();
    int64_t lastAttempt = _lastUpgradeAttempt.load();
    if (lastAttempt > 0 && (now - lastAttempt) < settings.upgradeDelayMs) {
        return false;
    }
    
    return true;
}
```

## 状态机实现

### Path Udp2Raw 状态机

```cpp
// State transitions
void PathUdp2Raw::startProbe() {
    if (_state.load() != UDP2RAW_PROBING) return;
    
    // Create socket
    _socket = std::make_unique<Udp2RawSocket>();
    if (!_socket->open(_localAddr, socketConfig)) {
        markFailed();
        return;
    }
    
    // Send probe
    if (_socket->sendProbe(_remoteAddr)) {
        _probeSent = true;
    }
}

void PathUdp2Raw::handleProbeResponse() {
    if (_state.load() == UDP2RAW_PROBING) {
        startHandshake();
    }
}

void PathUdp2Raw::startHandshake() {
    _state.store(UDP2RAW_HANDSHAKE);
    if (_socket->sendHandshake(_remoteAddr)) {
        _handshakeSent = true;
    } else {
        markFailed();
    }
}

void PathUdp2Raw::handleHandshakeAck(Udp2RawProtocol::TransportMode mode, uint16_t port) {
    if (_state.load() != UDP2RAW_HANDSHAKE) return;
    
    // Update transport type
    if (mode == Udp2RawProtocol::MODE_FAKE_TCP) {
        _transportType.store(TRANSPORT_UDP2RAW_TCP);
    } else {
        _transportType.store(TRANSPORT_UDP2RAW_ICMP);
    }
    
    _state.store(UDP2RAW_ESTABLISHED);
}

void PathUdp2Raw::markFailed() {
    _state.store(UDP2RAW_FAILED);
    _lastFailureTime.store(now);
    _transportType.store(TRANSPORT_UDP);
}
```

## 配置解析

### JSON 解析

```cpp
bool Udp2RawConfig::loadFromJson(const void* jsonConfig) {
    const nlohmann::json& config = *(const nlohmann::json*)jsonConfig;
    
    if (config.contains("mode")) {
        _settings.mode = parseMode(
            config["mode"].get<std::string>().c_str()
        );
    }
    
    if (config.contains("transport")) {
        _settings.transportMode = parseTransportMode(
            config["transport"].get<std::string>().c_str()
        );
    }
    
    if (config.contains("remotePort")) {
        _settings.remotePort = config["remotePort"].get<uint16_t>();
    }
    
    // ... more fields
    
    return true;
}
```

## 内存管理

### 智能指针使用

```cpp
class Path {
private:
    std::unique_ptr<PathUdp2Raw> _udp2Raw;
    
public:
    bool initUdp2Raw(const InetAddress& localAddr, const PathUdp2Raw::Config& config) {
        if (_udp2Raw) return true; // Already initialized
        _udp2Raw = std::make_unique<PathUdp2Raw>();
        return _udp2Raw->init(localAddr, _addr, config);
    }
    
    void shutdownUdp2Raw() {
        if (_udp2Raw) {
            _udp2Raw->shutdown();
            _udp2Raw.reset(); // Release memory
        }
    }
};
```

## 线程安全

### 原子变量

```cpp
class Udp2RawSocket {
private:
    std::atomic<State> _state;
    std::atomic<uint32_t> _tcpSeq;
    std::atomic<uint32_t> _tcpAck;
    std::atomic<uint64_t> _packetsSent;
    // ...
};
```

### 锁策略

- 状态检查使用原子变量（无锁）
- Socket 操作在单线程中执行
- 配置读取使用原子指针

## 错误处理

### 错误码

```cpp
enum class Udp2RawError {
    SUCCESS = 0,
    NO_PRIVILEGES,
    SOCKET_CREATE_FAILED,
    BIND_FAILED,
    SEND_FAILED,
    INVALID_PACKET,
    CHECKSUM_ERROR,
    TIMEOUT
};
```

### 错误恢复

```cpp
bool Udp2RawSocket::send(const InetAddress& remoteAddr, const void* data, unsigned int len) {
    if (_socket < 0) {
        _lastError = "Socket not open";
        return false;
    }
    
    ssize_t sent = ::sendto(_socket, packet, len, 0, ...);
    if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Would block, try again later
            return false;
        }
        // Fatal error
        _lastError = strerror(errno);
        _stats.errors.fetch_add(1);
        return false;
    }
    
    return true;
}
```

## 性能优化

### 批处理

```cpp
// Process multiple packets in one system call
int Udp2RawSocket::receiveBatch(void** bufs, unsigned int* lens, 
                                int maxPackets, int timeoutMs) {
    // Use recvmmsg for batch receive (Linux)
    struct mmsghdr msgs[MAX_BATCH];
    // ... setup
    return recvmmsg(_socket, msgs, maxPackets, 0, &timeout);
}
```

### 零拷贝

```cpp
// Use mmap for zero-copy packet access (advanced)
void* Udp2RawSocket::mapPacketBuffer() {
    // Requires PACKET_MMAP support
    return mmap(nullptr, size, PROT_READ|PROT_WRITE, MAP_SHARED, _socket, 0);
}
```

## 调试支持

### 日志宏

```cpp
#ifdef UDP2RAW_VERBOSE
#define UDP2RAW_LOG(fmt, ...) \
    fprintf(stderr, "[udp2raw] " fmt "\n", ##__VA_ARGS__)
#else
#define UDP2RAW_LOG(fmt, ...)
#endif
```

### 统计信息

```cpp
struct Stats {
    uint64_t packetsSent;
    uint64_t packetsReceived;
    uint64_t bytesSent;
    uint64_t bytesReceived;
    uint64_t errors;
    uint64_t checksumErrors;
    uint64_t handshakeSent;
    uint64_t handshakeReceived;
    uint64_t keepaliveSent;
    uint64_t keepaliveReceived;
};
```

## 代码规范

### 命名约定

- 类名: `Udp2RawProtocol`, `PathUdp2Raw`
- 方法: `send()`, `receive()`, `encapsulate()`
- 常量: `UDP2RAW_MAX_PACKET_SIZE`
- 枚举: `Udp2RawState`, `TransportType`

### 注释规范

```cpp
/**
 * Send data via udp2raw
 * @param remoteAddr Destination address
 * @param data Data to send
 * @param len Data length
 * @return true on success
 * @note Requires open() to be called first
 */
bool send(const InetAddress& remoteAddr, const void* data, unsigned int len);
```

## 版本历史

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0 | 2026-03-29 | 初始实现 |
