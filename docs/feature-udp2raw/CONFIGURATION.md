# Udp2Raw 配置指南

## 配置文件位置

```
/var/lib/zerotier-one/local.conf
```

## 完整配置示例

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

## 配置项说明

### mode

**类型**: string  
**默认值**: `"auto"`  
**可选值**: `"disabled"`, `"auto"`, `"preferred"`, `"required"`

| 值 | 说明 |
|----|------|
| `disabled` | 完全禁用 udp2raw |
| `auto` | 自动检测并升级 (推荐) |
| `preferred` | 优先使用 udp2raw |
| `required` | 强制使用 udp2raw (失败则不通) |

### transport

**类型**: string  
**默认值**: `"tcp"`  
**可选值**: `"tcp"`, `"icmp"`

| 值 | 说明 |
|----|------|
| `tcp` | 使用 FakeTCP 封装 |
| `icmp` | 使用 ICMP 封装 |

### localPort

**类型**: integer  
**默认值**: `0` (自动选择)

本地 FakeTCP 端口。0 表示自动选择。

### remotePort

**类型**: integer  
**默认值**: `443`

远程 FakeTCP 端口。常用值：
- `443` - HTTPS (推荐)
- `80` - HTTP
- `8080` - HTTP 代理
- `22` - SSH
- `53` - DNS

### icmpId

**类型**: integer  
**默认值**: `4660` (0x1234)

ICMP 标识符。用于 ICMP 模式。

### probeInterval

**类型**: integer  
**默认值**: `30000` (30秒)

探测对端能力的间隔时间（毫秒）。

### handshakeTimeout

**类型**: integer  
**默认值**: `10000` (10秒)

握手超时时间（毫秒）。

### fallbackDelay

**类型**: integer  
**默认值**: `60000` (60秒)

失败后回退到 UDP 的延迟时间（毫秒）。

### upgradeDelay

**类型**: integer  
**默认值**: `5000` (5秒)

路径建立后升级到 udp2raw 的延迟时间（毫秒）。

### autoUpgrade

**类型**: boolean  
**默认值**: `true`

是否自动升级到 udp2raw。

### autoDowngrade

**类型**: boolean  
**默认值**: `true`

失败时是否自动降级到 UDP。

### logVerbose

**类型**: boolean  
**默认值**: `false`

是否启用详细日志。

### fakeTcpPorts

**类型**: string  
**默认值**: `"443,80,8080,8443,53,22"`

逗号分隔的 TCP 端口列表，用于尝试 FakeTCP 连接。

## 配置场景

### 场景 1: 默认配置 (推荐)

```json
{
    "settings": {
        "udp2raw": {
            "mode": "auto"
        }
    }
}
```

自动检测对端支持，自动升级。

### 场景 2: 强制使用 udp2raw

```json
{
    "settings": {
        "udp2raw": {
            "mode": "required",
            "transport": "tcp",
            "remotePort": 443
        }
    }
}
```

注意：如果对端不支持 udp2raw，连接将失败。

### 场景 3: 使用 ICMP 模式

```json
{
    "settings": {
        "udp2raw": {
            "mode": "auto",
            "transport": "icmp",
            "icmpId": 4660
        }
    }
}
```

适用于 TCP 也被限制的环境。

### 场景 4: 多端口尝试

```json
{
    "settings": {
        "udp2raw": {
            "mode": "auto",
            "transport": "tcp",
            "fakeTcpPorts": "443,80,8080,8443,53,22,25,587,993,995"
        }
    }
}
```

尝试多个常用端口。

### 场景 5: 禁用 udp2raw

```json
{
    "settings": {
        "udp2raw": {
            "mode": "disabled"
        }
    }
}
```

完全禁用 udp2raw 功能。

### 场景 6: 调试模式

```json
{
    "settings": {
        "udp2raw": {
            "mode": "auto",
            "logVerbose": true,
            "probeInterval": 5000,
            "handshakeTimeout": 5000
        }
    }
}
```

启用详细日志，缩短超时时间。

## 配置验证

### 检查配置是否生效

```bash
sudo journalctl -u zerotier-one -f | grep udp2raw
```

预期输出：
```
[udp2raw] Enabled (mode: auto)
[udp2raw] Transport: tcp, Remote port: 443
```

### 检查路径状态

```bash
sudo zerotier-cli peers
```

查看路径是否有 udp2raw 标记。

## 故障排除

### 配置不生效

1. 检查 JSON 格式是否正确
2. 检查文件权限
3. 重启服务

```bash
sudo systemctl restart zerotier-one
```

### 权限问题

```bash
# 检查权限
sudo getcap /usr/sbin/zerotier-one

# 添加权限
sudo setcap cap_net_raw+ep /usr/sbin/zerotier-one

# 或使用 root 运行
sudo zerotier-one
```

## 配置文件模板

### 最小配置

```json
{}
```

使用所有默认值。

### 完整配置

见本文档开头的完整配置示例。

### 生产环境配置

```json
{
    "settings": {
        "udp2raw": {
            "mode": "auto",
            "transport": "tcp",
            "remotePort": 443,
            "fallbackDelay": 300000,
            "autoUpgrade": true,
            "autoDowngrade": true,
            "logVerbose": false
        }
    }
}
```

- 使用 TCP 443 端口
- 5分钟回退延迟
- 关闭详细日志
