# Udp2Raw 故障排除指南

## 常见问题

### 1. Udp2Raw 未启用

**症状**：日志中没有 `[udp2raw]` 相关输出

**检查**：
```bash
# 检查配置
cat /var/lib/zerotier-one/local.conf | grep -A 10 udp2raw

# 检查日志
sudo journalctl -u zerotier-one -f | grep -i udp2raw
```

**解决**：
1. 确保配置文件存在且格式正确
2. 重启服务
```bash
sudo systemctl restart zerotier-one
```

### 2. 权限不足

**症状**：
```
[udp2raw] WARNING: No privileges for raw sockets
```

**检查**：
```bash
# 检查当前用户
whoami

# 检查文件权限
getcap /usr/sbin/zerotier-one
```

**解决**：

方案 1 - 使用 root：
```bash
sudo zerotier-one
```

方案 2 - 添加能力：
```bash
sudo setcap cap_net_raw+ep /usr/sbin/zerotier-one
```

方案 3 - 使用 sudo：
```bash
sudo systemctl edit zerotier-one
# 添加：
[Service]
User=root
```

### 3. 平台不支持

**症状**：
```
[udp2raw] WARNING: Raw sockets not supported on this platform
```

**原因**：Windows 不支持原生 Raw Socket

**解决**：
- 使用 Linux 或 macOS
- Windows 需要额外的 WinDivert 支持（未实现）

### 4. 连接未升级到 Udp2Raw

**症状**：Peer 仍使用标准 UDP

**检查**：
```bash
# 检查对端是否支持
zerotier-cli peers

# 检查路径状态
# 应该有 udp2raw 标记
```

**可能原因**：
1. 对端未启用 udp2raw
2. 握手失败
3. 防火墙阻断

**解决**：
1. 确保对端也启用了 udp2raw
2. 检查防火墙规则
3. 尝试不同端口

### 5. 握手超时

**症状**：
```
[udp2raw] Handshake timeout
```

**检查**：
```bash
# 检查网络连通性
ping <peer-ip>
telnet <peer-ip> 443
```

**解决**：
1. 增加超时时间：
```json
{
    "settings": {
        "udp2raw": {
            "handshakeTimeout": 30000
        }
    }
}
```

2. 尝试其他端口：
```json
{
    "settings": {
        "udp2raw": {
            "remotePort": 80
        }
    }
}
```

### 6. 频繁回退到 UDP

**症状**：Udp2Raw 连接不稳定，经常回退

**检查**：
```bash
# 检查网络质量
ping -c 100 <peer-ip>
# 查看丢包率

# 检查日志
sudo journalctl -u zerotier-one -f | grep -i "fallback\|failed"
```

**可能原因**：
1. 网络不稳定
2. 防火墙干扰
3. 配置不当

**解决**：
1. 增加回退延迟：
```json
{
    "settings": {
        "udp2raw": {
            "fallbackDelay": 300000
        }
    }
}
```

2. 使用 ICMP 模式：
```json
{
    "settings": {
        "udp2raw": {
            "transport": "icmp"
        }
    }
}
```

### 7. 性能下降

**症状**：启用 udp2raw 后延迟增加或吞吐量下降

**检查**：
```bash
# 测试延迟
ping <zerotier-ip>

# 测试吞吐量
iperf3 -c <zerotier-ip>
```

**可能原因**：
1. CPU 开销
2. 封装开销
3. 网络路径变化

**解决**：
1. 禁用详细日志
2. 调整 MTU
3. 考虑禁用 udp2raw（如果不需要）

### 8. 配置不生效

**症状**：修改配置后无变化

**检查**：
```bash
# 检查配置文件语法
python3 -m json.tool /var/lib/zerotier-one/local.conf

# 检查文件权限
ls -la /var/lib/zerotier-one/local.conf
```

**解决**：
1. 确保 JSON 格式正确
2. 重启服务
3. 检查配置路径

## 调试方法

### 启用详细日志

```json
{
    "settings": {
        "udp2raw": {
            "logVerbose": true
        }
    }
}
```

### 使用 tcpdump

```bash
# 捕获 udp2raw 流量
sudo tcpdump -i any -n host <peer-ip> and port 443 -w udp2raw.pcap

# 分析
wireshark udp2raw.pcap
```

### 使用 strace

```bash
# 跟踪系统调用
sudo strace -f -e trace=network zerotier-one 2>&1 | grep -i udp2raw
```

### 使用 gdb

```bash
# 调试运行
sudo gdb zerotier-one

# 设置断点
break Udp2RawSocket::send
break Path::send

# 运行
run

# 查看变量
print _state
print _udp2Raw
```

## 日志分析

### 正常日志

```
[udp2raw] Enabled (mode: auto)
[udp2raw] Path upgraded: 192.168.1.100/9993
[udp2raw] Handshake completed
```

### 异常日志

```
[udp2raw] Failed to create raw socket: Permission denied
[udp2raw] Handshake timeout
[udp2raw] Fallback to UDP
```

## 诊断脚本

```bash
#!/bin/bash
# udp2raw-diagnose.sh

echo "=== Udp2Raw Diagnostic ==="
echo ""

echo "1. Checking configuration..."
if [ -f /var/lib/zerotier-one/local.conf ]; then
    echo "  Config file exists"
    python3 -m json.tool /var/lib/zerotier-one/local.conf > /dev/null 2>&1
    if [ $? -eq 0 ]; then
        echo "  Config syntax: OK"
    else
        echo "  Config syntax: FAILED"
    fi
else
    echo "  Config file: NOT FOUND"
fi

echo ""
echo "2. Checking permissions..."
if [ "$(id -u)" -eq 0 ]; then
    echo "  Running as root: YES"
else
    echo "  Running as root: NO"
fi

cap=$(getcap /usr/sbin/zerotier-one 2>/dev/null)
if echo "$cap" | grep -q "cap_net_raw"; then
    echo "  CAP_NET_RAW: YES"
else
    echo "  CAP_NET_RAW: NO"
fi

echo ""
echo "3. Checking platform..."
if [ "$(uname)" = "Linux" ]; then
    echo "  Platform: Linux (supported)"
elif [ "$(uname)" = "Darwin" ]; then
    echo "  Platform: macOS (supported)"
else
    echo "  Platform: $(uname) (may not be supported)"
fi

echo ""
echo "4. Checking recent logs..."
sudo journalctl -u zerotier-one --since "1 hour ago" | grep -i udp2raw | tail -20

echo ""
echo "=== End of Diagnostic ==="
```

## 联系支持

如果以上方法无法解决问题：

1. 收集日志：
```bash
sudo journalctl -u zerotier-one --since "24 hours ago" > zerotier-logs.txt
```

2. 收集配置：
```bash
cat /var/lib/zerotier-one/local.conf > zerotier-config.txt
```

3. 提交 Issue：
- 描述问题
- 提供日志
- 提供配置
- 说明环境（OS, 版本等）

## 已知限制

### 平台限制
- Windows 不支持（需要 WinDivert）
- 某些容器环境不支持 Raw Socket

### 功能限制
- 不支持 IPv6（当前实现）
- 不支持 TCP 选项协商
- 不支持分片

### 性能限制
- 比原生 UDP 略高的 CPU 占用
- 略高的延迟（1-2ms）

## 参考资源

- [Udp2Raw 原始项目](https://github.com/wangyu-/udp2raw)
- [ZeroTier 文档](https://docs.zerotier.com/)
- [Raw Socket 编程](https://man7.org/linux/man-pages/man7/raw.7.html)
