# Udp2Raw 架构设计

## 总体架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           ZeroTier Node                                 │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌──────────┐ │
│  │   Switch    │───▶│    Peer     │───▶│    Path     │───▶│  Socket  │ │
│  │             │    │             │    │             │    │          │ │
│  │             │    │ ┌─────────┐ │    │ ┌─────────┐ │    │ ┌──────┐ │ │
│  │             │    │ │Peer     │ │    │ │Path     │ │    │ │Udp2  │ │ │
│  │             │    │ │Udp2Raw  │ │    │ │Udp2Raw  │ │    │ │Raw   │ │ │
│  │             │    │ └─────────┘ │    │ └─────────┘ │    │ └──────┘ │ │
│  └─────────────┘    └─────────────┘    └─────────────┘    └──────────┘ │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
                           ┌─────────────────┐
                           │  Raw Socket     │
                           │ (FakeTCP/ICMP)  │
                           └─────────────────┘
                                    │
                                    ▼
                              [Internet]
                                    │
                           [Udp2Raw Server]
                                    │
                           [ZeroTier Peer]
```

## 分层架构

### 1. 协议层 (Protocol Layer)

**文件**: `node/Udp2RawProtocol.hpp/cpp`

负责 ZeroTier 数据包的封装和解封装。

#### 功能
- FakeTCP 封装/解封装
- ICMP 封装/解封装
- TCP/ICMP 校验和计算
- 握手包、保活包、探测包生成

#### 数据结构

```cpp
// Udp2Raw 包头
struct PacketHeader {
    uint32_t magic;       // 魔数 0x52415732 ("RAW2")
    uint8_t version;      // 协议版本 (1)
    uint8_t packetType;   // 包类型
    uint16_t payloadLen;  // 载荷长度
    uint32_t seqNum;      // TCP 序列号
    uint32_t ackNum;      // TCP 确认号
    uint8_t options;      // 选项标志
};

// FakeTCP 包头
struct TcpHeader {
    uint16_t sourcePort;
    uint16_t destPort;
    uint32_t seqNum;
    uint32_t ackNum;
    uint8_t dataOffset;
    uint8_t flags;        // SYN, ACK, PSH 等
    uint16_t windowSize;
    uint16_t checksum;
    uint16_t urgentPtr;
};

// ICMP 包头
struct IcmpHeader {
    uint8_t type;         // 8 (Echo Request) 或 0 (Echo Reply)
    uint8_t code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
};
```

#### 包类型

| 类型 | 值 | 说明 |
|------|-----|------|
| PACKET_TYPE_DATA | 0x01 | 数据包 |
| PACKET_TYPE_HANDSHAKE | 0x02 | 握手请求 |
| PACKET_TYPE_HANDSHAKE_ACK | 0x03 | 握手响应 |
| PACKET_TYPE_KEEPALIVE | 0x04 | 保活包 |
| PACKET_TYPE_PROBE | 0x05 | 能力探测 |

### 2. Socket 层 (Socket Layer)

**文件**: `osdep/Udp2RawSocket.hpp/cpp`

负责 Raw Socket 的创建、发送和接收。

#### 功能
- Raw Socket 创建和管理
- 发送 FakeTCP/ICMP 包
- 接收并解析 Raw 包
- 握手和保活处理

#### 状态机

```
STATE_CLOSED ──open()──▶ STATE_OPENING ──成功──▶ STATE_OPEN
                              │
                              └──失败──▶ STATE_ERROR
```

#### TCP 状态

```
TCP_CLOSED ──send SYN──▶ TCP_SYN_SENT ──recv SYN-ACK──▶ TCP_ESTABLISHED
```

### 3. Path 层 (Path Layer)

**文件**: `node/PathUdp2Raw.hpp/cpp`, `node/Path.hpp/cpp` (修改)

负责单条路径的 udp2raw 管理。

#### 功能
- 路径级 udp2raw 初始化
- 发送决策（udp2raw vs UDP）
- 接收包处理
- 状态管理

#### 传输类型

```cpp
enum TransportType {
    TRANSPORT_UDP = 0,           // 标准 UDP
    TRANSPORT_UDP2RAW_TCP,       // FakeTCP 封装
    TRANSPORT_UDP2RAW_ICMP       // ICMP 封装
};
```

#### Udp2Raw 状态

```cpp
enum Udp2RawState {
    UDP2RAW_DISABLED = 0,   // 未启用
    UDP2RAW_PROBING,        // 探测对端能力
    UDP2RAW_HANDSHAKE,      // 握手协商
    UDP2RAW_ESTABLISHED,    // 已建立
    UDP2RAW_FAILED          // 失败
};
```

#### 状态流转

```
UDP2RAW_DISABLED
       │
       │ init() with enabled=true
       ▼
UDP2RAW_PROBING ──send probe──▶ wait response
       │
       │ recv probe response
       ▼
UDP2RAW_HANDSHAKE ──send handshake──▶ wait ACK
       │
       │ recv handshake ACK
       ▼
UDP2RAW_ESTABLISHED ◀───────┐
       │                      │
       │ failure              │ success
       ▼                      │
UDP2RAW_FAILED ──retry──▶────┘
```

### 4. Peer 层 (Peer Layer)

**文件**: `node/PeerUdp2Raw.hpp/cpp`

负责 Peer 级别的 udp2raw 管理。

#### 功能
- 路径升级决策
- 失败重试管理
- 多路径 udp2raw 统计

#### 失败重试逻辑

```cpp
void markFailed(int64_t now) {
    _failed = true;
    _lastFailureTime = now;
}

bool canRetry(int64_t now) const {
    if (!_failed) return true;
    return (now - _lastFailureTime) > fallbackDelayMs;
}
// fallbackDelayMs = 60000ms (1分钟)
```

### 5. 配置层 (Config Layer)

**文件**: `node/Udp2RawConfig.hpp/cpp`

负责全局配置管理。

#### 配置模式

```cpp
enum GlobalMode {
    MODE_DISABLED = 0,   // 完全禁用
    MODE_AUTO,           // 自动检测和升级 (默认)
    MODE_PREFERRED,      // 优先使用 udp2raw
    MODE_REQUIRED        // 强制使用 udp2raw
};
```

#### 默认配置

```json
{
    "settings": {
        "udp2raw": {
            "mode": "auto",
            "transport": "tcp",
            "localPort": 0,
            "remotePort": 443,
            "icmpId": 4660,
            "probeInterval": 30000,
            "handshakeTimeout": 10000,
            "fallbackDelay": 60000,
            "upgradeDelay": 5000,
            "autoUpgrade": true,
            "autoDowngrade": true,
            "logVerbose": false,
            "fakeTcpPorts": "443,80,8080,8443,53,22"
        }
    }
}
```

## 数据流

### 发送流程

```
┌─────────┐     ┌─────────┐     ┌─────────┐     ┌─────────┐     ┌─────────┐
│  App    │───▶│  Peer   │───▶│  Path   │───▶│  Socket │───▶│  Raw    │
│  Data   │     │  send() │     │  send() │     │  send() │     │  Socket │
└─────────┘     └─────────┘     └─────────┘     └─────────┘     └─────────┘
                                    │
                                    │ hasUdp2Raw()?
                                    ├─yes──▶ encapsulate() ──▶ sendto()
                                    │
                                    └─no───▶ standard UDP send
```

### 接收流程

```
┌─────────┐     ┌─────────┐     ┌─────────┐     ┌─────────┐     ┌─────────┐
│  Raw    │───▶│  Socket │───▶│  Path   │───▶│  Peer   │───▶│  App    │
│  Socket │     │  recv() │     │process()│     │received()│     │  Data   │
└─────────┘     └─────────┘     └─────────┘     └─────────┘     └─────────┘
                                    │
                                    │ is udp2raw packet?
                                    ├─yes──▶ decapsulate() ──▶ process
                                    │
                                    └─no───▶ standard UDP process
```

## 前向兼容设计

### 协议兼容
- Magic Number + Version 检查
- 未知版本自动回退
- 可选功能，不影响标准 UDP

### 运行时兼容
- 无权限自动回退 UDP
- 对端不支持透明回退
- 失败自动降级

### 配置兼容
- 默认启用但不强制
- 可完全禁用
- 不影响现有配置

## 性能考虑

### 开销
- 封装开销: ~40 bytes (IP + TCP header)
- CPU 开销: 校验和计算
- 延迟增加: ~1-2ms (本地处理)

### 优化
- 批量处理减少系统调用
- 非阻塞 I/O
- 连接状态缓存

## 安全考虑

### 权限要求
- 需要 CAP_NET_RAW 或 root
- Windows 需要 WinDivert (未实现)

### 数据安全
- ZeroTier 加密保持不变
- 仅改变传输层封装
- 不暴露原始 UDP 端口

## 平台支持

| 平台 | 支持状态 | 说明 |
|------|---------|------|
| Linux | ✅ 支持 | 完整支持 |
| macOS | ⚠️ 部分 | 可能需要额外配置 |
| Windows | ❌ 不支持 | 需要 WinDivert |
| FreeBSD | ⚠️ 未测试 | 理论上支持 |
| OpenBSD | ⚠️ 未测试 | 理论上支持 |
