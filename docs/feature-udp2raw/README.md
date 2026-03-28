# ZeroTier Udp2Raw 功能文档

## 概述

Udp2Raw 是 ZeroTier 的一个扩展功能，用于将 UDP 流量伪装成 TCP 或 ICMP 流量，从而绕过限制 UDP 的防火墙和 QoS 限速。

## 目录结构

```
docs/feature-udp2raw/
├── README.md                 # 本文档
├── ARCHITECTURE.md           # 架构设计
├── IMPLEMENTATION.md         # 实现细节
├── CONFIGURATION.md          # 配置指南
├── TESTING.md                # 测试说明
├── INTEGRATION.md            # 集成指南
└── TROUBLESHOOTING.md        # 故障排除
```

## 快速开始

### 1. 编译安装

```bash
cd /path/to/ZeroTierOne
make clean && make
sudo make install
```

### 2. 配置

创建 `/var/lib/zerotier-one/local.conf`:

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

### 3. 启动

```bash
sudo systemctl restart zerotier-one
sudo journalctl -u zerotier-one -f
```

查看日志中的 `[udp2raw]` 相关输出。

## 功能特性

- **自动检测**: 自动探测对端是否支持 udp2raw
- **无缝回退**: 失败时自动回退到标准 UDP
- **多模式支持**: 支持 FakeTCP 和 ICMP 两种封装模式
- **零配置**: 默认启用，无需额外配置
- **前向兼容**: 不影响不支持 udp2raw 的对端

## 系统要求

- Linux (内核 3.x 或更高)
- Root 权限 或 CAP_NET_RAW 能力
- 双方节点都启用 udp2raw 才能使用

## 许可证

与 ZeroTier 相同，采用 GPLv3 许可证。
