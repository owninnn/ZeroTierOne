# Udp2Raw 集成指南

## 概述

本文档描述如何将 Udp2Raw 功能集成到 ZeroTier 中。

## 集成步骤

### 1. 添加源文件

将以下文件添加到项目中：

**node 目录：**
- `Udp2RawProtocol.hpp`
- `Udp2RawProtocol.cpp`
- `Udp2RawConfig.hpp`
- `Udp2RawConfig.cpp`
- `PathUdp2Raw.hpp`
- `PathUdp2Raw.cpp`
- `PeerUdp2Raw.hpp`
- `PeerUdp2Raw.cpp`

**osdep 目录：**
- `Udp2RawSocket.hpp`
- `Udp2RawSocket.cpp`

### 2. 修改现有文件

#### Path.hpp

添加包含：
```cpp
#include "PathUdp2Raw.hpp"
#include <memory>
```

添加成员：
```cpp
private:
    std::unique_ptr<PathUdp2Raw> _udp2Raw;

public:
    bool initUdp2Raw(const InetAddress& localAddr, const PathUdp2Raw::Config& config);
    inline bool hasUdp2Raw() const { return _udp2Raw && _udp2Raw->isActive(); }
    inline PathUdp2Raw::Udp2RawState getUdp2RawState() const { ... }
    bool sendUdp2Raw(const void* data, unsigned int len);
    bool processUdp2RawPacket(const void* data, unsigned int len);
    void shutdownUdp2Raw();
```

#### Path.cpp

修改 `send()` 方法：
```cpp
bool Path::send(const RuntimeEnvironment* RR, void* tPtr, const void* data, unsigned int len, int64_t now)
{
    // Try udp2raw first if available
    if (hasUdp2Raw()) {
        if (_udp2Raw->send(data, len)) {
            _lastOut = now;
            return true;
        }
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
```

添加新方法实现（见 IMPLEMENTATION.md）。

### 3. 修改 OneService.cpp

添加包含：
```cpp
#include "../node/Udp2RawConfig.hpp"
#include "../osdep/Udp2RawSocket.hpp"
```

在配置加载部分添加：
```cpp
// Udp2Raw configuration
json& udp2raw = settings["udp2raw"];
if (udp2raw.is_object()) {
    Udp2RawConfig::getInstance().loadFromJson(&udp2raw);
    
    if (Udp2RawConfig::getInstance().isEnabled()) {
        fprintf(stderr, "[udp2raw] Enabled (mode: %s)\n", 
            Udp2RawConfig::getModeName(Udp2RawConfig::getInstance().getSettings().mode));
        
        if (!Udp2RawSocket::isSupported()) {
            fprintf(stderr, "[udp2raw] WARNING: Raw sockets not supported\n");
        }
        if (!Udp2RawSocket::hasPrivileges()) {
            fprintf(stderr, "[udp2raw] WARNING: No privileges\n");
        }
    }
}
```

### 4. 更新 Makefile

#### node/objects.mk

添加：
```makefile
OBJS += node/Udp2RawProtocol.o \
        node/Udp2RawConfig.o \
        node/PathUdp2Raw.o \
        node/PeerUdp2Raw.o
```

#### osdep/objects.mk

添加：
```makefile
OBJS += osdep/Udp2RawSocket.o
```

### 5. 应用补丁

使用提供的补丁文件：

```bash
cd /path/to/zerotier-one
cp service/OneService.cpp service/OneService.cpp.bak
patch -p1 < service/OneService_udp2raw.patch
```

## 构建

### 完整构建

```bash
make clean
make
sudo make install
```

### 仅构建测试

```bash
cd node
make -f Udp2RawProtocolTest.mk
cd ../osdep
make -f Udp2RawSocketTest.mk
```

## 配置

### 最小配置

```json
{
    "settings": {
        "udp2raw": {
            "mode": "auto"
        }
    }
}
```

### 完整配置

见 CONFIGURATION.md。

## 验证

### 1. 检查编译

```bash
# 检查对象文件是否存在
ls -la node/Udp2RawProtocol.o
ls -la osdep/Udp2RawSocket.o
```

### 2. 检查日志

```bash
sudo journalctl -u zerotier-one -f | grep udp2raw
```

预期输出：
```
[udp2raw] Enabled (mode: auto)
```

### 3. 检查功能

```bash
# 检查版本
zerotier-cli -v

# 检查状态
zerotier-cli status

# 检查 peers
zerotier-cli peers
```

## 故障排除

### 编译错误

#### 找不到头文件

```
error: 'nlohmann/json.hpp' file not found
```

**解决**：确保 JSON 库已安装或包含路径正确。

#### 链接错误

```
undefined reference to 'ZeroTier::Udp2RawProtocol::...'
```

**解决**：确保所有对象文件已添加到 Makefile。

### 运行时错误

#### 权限错误

```
[udp2raw] WARNING: No privileges for raw sockets
```

**解决**：
```bash
sudo setcap cap_net_raw+ep /usr/sbin/zerotier-one
# 或
sudo zerotier-one
```

#### 不支持的平台

```
[udp2raw] WARNING: Raw sockets not supported on this platform
```

**解决**：使用 Linux 或 macOS。Windows 需要 WinDivert。

## 回滚

如果需要移除 Udp2raw 功能：

### 1. 恢复 OneService.cpp

```bash
cp service/OneService.cpp.bak service/OneService.cpp
```

### 2. 恢复 Path.hpp/cpp

从 git 恢复：
```bash
git checkout node/Path.hpp node/Path.cpp
```

### 3. 移除对象文件

编辑 Makefile，移除 udp2raw 相关的对象文件。

### 4. 重新编译

```bash
make clean && make
sudo make install
```

## 高级集成

### 自定义配置加载

```cpp
// 在 OneService 中添加自定义配置处理
void loadCustomUdp2RawConfig(const json& config) {
    // 自定义配置解析
}
```

### 事件回调

```cpp
// 添加状态变更回调
class Udp2RawCallbacks {
public:
    virtual void onStateChange(PathUdp2Raw::Udp2RawState oldState, 
                                PathUdp2Raw::Udp2RawState newState) = 0;
    virtual void onPacketSent(size_t bytes) = 0;
    virtual void onPacketReceived(size_t bytes) = 0;
};
```

### 指标收集

```cpp
// 集成到现有指标系统
void collectUdp2RawMetrics() {
    auto stats = _udp2RawSocket.getStats();
    Metrics::udp2raw_packets_sent += stats.packetsSent;
    Metrics::udp2raw_packets_received += stats.packetsReceived;
}
```

## 最佳实践

### 1. 渐进式部署

1. 先在测试环境验证
2. 小规模生产环境试点
3. 逐步推广到所有节点

### 2. 监控

- 监控 udp2raw 连接成功率
- 监控回退到 UDP 的频率
- 监控延迟和吞吐量变化

### 3. 配置管理

- 使用配置管理工具（Ansible/Puppet）
- 版本控制配置文件
- 配置变更审计

## 与其他功能的交互

### 与 Multipath 的交互

- Udp2raw 路径可以与普通 UDP 路径同时使用
- Bond 层会将 udp2raw 路径视为普通路径

### 与 Relay 的交互

- Udp2raw 仅用于直连路径
- 中继连接仍使用标准 UDP

### 与 SSO 的交互

- 无直接影响
- 独立的功能模块

## 安全考虑

### 权限最小化

```bash
# 仅授予必要权限
sudo setcap cap_net_raw+ep /usr/sbin/zerotier-one

# 不要以 root 运行（如果可能）
```

### 审计日志

```cpp
// 记录重要事件
syslog(LOG_INFO, "[udp2raw] Path upgraded: %s", path->address().toString());
```

## 性能优化

### 编译优化

```bash
# 使用 O3 优化
make CXXFLAGS="-O3 -DNDEBUG"
```

### 运行时优化

- 调整 `probeInterval`
- 调整 `handshakeTimeout`
- 启用批处理（如果支持）
