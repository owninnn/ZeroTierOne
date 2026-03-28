# Udp2Raw 测试说明

## 测试概述

Udp2Raw 包含多层次的测试，从单元测试到集成测试。

## 测试文件清单

| 测试文件 | 说明 | 依赖 |
|----------|------|------|
| `node/Udp2RawProtocolTest.cpp` | 协议层单元测试 | 无 |
| `osdep/Udp2RawSocketTest.cpp` | Socket 层单元测试 | 无 |
| `node/PathUdp2RawTest.cpp` | Path 层单元测试 | 无 |
| `node/PathIntegrationTest.cpp` | 集成测试 | Config |
| `node/PeerUdp2RawStandaloneTest.cpp` | Peer 层测试 | Config |

## 运行测试

### 协议层测试

```bash
cd node
make -f Udp2RawProtocolTest.mk clean
make -f Udp2RawProtocolTest.mk
./udp2raw_protocol_test
```

预期输出：
```
======================================
Udp2RawProtocol Unit Tests
======================================

Test: Header basic initialization and validation... PASSED
Test: Checksum calculation... PASSED
Test: FakeTCP encapsulation/decapsulation... PASSED
Test: ICMP encapsulation/decapsulation... PASSED
Test: Handshake packet creation... PASSED
Test: Keepalive and probe packets... PASSED
Test: Edge cases... PASSED

All tests PASSED!
```

### Socket 层测试

```bash
cd osdep
make -f Udp2RawSocketTest.mk clean
make -f Udp2RawSocketTest.mk
./udp2raw_socket_test
```

预期输出：
```
======================================
Udp2RawSocket Unit Tests
======================================

Test: Basic functionality... PASSED
Test: Configuration... PASSED

All tests PASSED!
```

### Path 层测试

```bash
cd node
make -f PathUdp2RawTest.mk clean
make -f PathUdp2RawTest.mk
./path_udp2raw_test
```

预期输出：
```
======================================
PathUdp2Raw Unit Tests
======================================

Test: Basic functionality... PASSED
Test: Configuration... PASSED
Test: State transitions... PASSED
Test: Statistics... PASSED

All tests PASSED!
```

### 集成测试

```bash
cd node
make -f PathIntegrationTest.mk clean
make -f PathIntegrationTest.mk
./path_integration_test
```

预期输出：
```
======================================
Path Integration Test with Udp2Raw
======================================

Test: Udp2RawConfig modes... PASSED
Test: Transport mode parsing... PASSED
Test: Udp2RawConfig defaults... PASSED
Test: Udp2RawConfig checks... PASSED
Test: Default config snippet... PASSED
Test: PathUdp2Raw lifecycle... PASSED

All tests PASSED!
```

### Peer 层测试

```bash
cd node
make -f PeerUdp2RawStandaloneTest.mk clean
make -f PeerUdp2RawStandaloneTest.mk
./peer_udp2raw_standalone_test
```

预期输出：
```
======================================
PeerUdp2Raw Standalone Tests
======================================

Test: Failure handling... PASSED
Test: Global config... PASSED
Test: Mode parsing... PASSED
Test: Transport mode parsing... PASSED

All tests PASSED!
```

## 端到端测试

### 运行所有测试

```bash
./test_udp2raw_integration.sh
```

### 手动端到端测试

#### 测试环境准备

1. 准备两台 Linux 机器 (A 和 B)
2. 确保都有 root 权限
3. 安装编译好的 ZeroTier

#### 测试步骤

**在机器 A 上：**

```bash
# 创建配置
sudo tee /var/lib/zerotier-one/local.conf > /dev/null << 'EOF'
{
    "settings": {
        "udp2raw": {
            "mode": "auto",
            "transport": "tcp",
            "remotePort": 443,
            "logVerbose": true
        }
    }
}
EOF

# 重启服务
sudo systemctl restart zerotier-one

# 查看日志
sudo journalctl -u zerotier-one -f | grep udp2raw
```

**在机器 B 上：**

```bash
# 同样配置
sudo tee /var/lib/zerotier-one/local.conf > /dev/null << 'EOF'
{
    "settings": {
        "udp2raw": {
            "mode": "auto",
            "transport": "tcp",
            "remotePort": 443,
            "logVerbose": true
        }
    }
}
EOF

sudo systemctl restart zerotier-one
```

**验证连接：**

```bash
# 在任意机器上
sudo zerotier-cli peers

# 查看路径详情
sudo zerotier-cli listpeers
```

预期看到：
- 连接状态为 `DIRECT` 或 `RELAY`
- 日志中出现 udp2raw 相关输出

#### 测试 UDP 阻断环境

使用 iptables 模拟 UDP 阻断：

```bash
# 在机器 A 上阻断 UDP (测试环境)
sudo iptables -A INPUT -p udp --dport 9993 -j DROP

# 测试连接
ping <zerotier-ip-of-B>

# 应该仍然能通 (通过 udp2raw)

# 恢复
sudo iptables -D INPUT -p udp --dport 9993 -j DROP
```

## 性能测试

### 吞吐量测试

```bash
# 使用 iperf3
iperf3 -s  # 在机器 B
iperf3 -c <zerotier-ip-of-B>  # 在机器 A
```

### 延迟测试

```bash
# 使用 ping
ping <zerotier-ip-of-B>
```

### 对比测试

1. 禁用 udp2raw，测试标准 UDP 性能
2. 启用 udp2raw，测试 FakeTCP 性能
3. 比较延迟和吞吐量

## 压力测试

### 长时间运行测试

```bash
# 持续运行 24 小时
# 监控稳定性
```

### 高并发测试

```bash
# 同时建立多个连接
# 测试资源占用
```

## 故障注入测试

### 网络故障

```bash
# 模拟丢包
tc qdisc add dev eth0 root netem loss 10%

# 模拟延迟
tc qdisc add dev eth0 root netem delay 100ms

# 恢复
tc qdisc del dev eth0 root
```

### 权限故障

```bash
# 移除权限
sudo setcap cap_net_raw-ep /usr/sbin/zerotier-one

# 测试回退

# 恢复权限
sudo setcap cap_net_raw+ep /usr/sbin/zerotier-one
```

## 测试覆盖率

### 代码覆盖率

```bash
# 编译时添加覆盖率选项
CXXFLAGS="-fprofile-arcs -ftest-coverage"

# 运行测试
# 生成报告
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_report
```

## 持续集成

### GitHub Actions 示例

```yaml
name: Udp2Raw Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v2
    
    - name: Build
      run: make
    
    - name: Run Protocol Tests
      run: |
        cd node
        make -f Udp2RawProtocolTest.mk
        ./udp2raw_protocol_test
    
    - name: Run Socket Tests
      run: |
        cd osdep
        make -f Udp2RawSocketTest.mk
        ./udp2raw_socket_test
    
    - name: Run Integration Tests
      run: |
        cd node
        make -f PathIntegrationTest.mk
        ./path_integration_test
```

## 调试测试

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

### 使用 gdb

```bash
gdb ./udp2raw_protocol_test
run
bt  # 查看调用栈
```

### 使用 valgrind

```bash
valgrind --leak-check=full ./udp2raw_protocol_test
```

## 测试检查清单

### 功能测试

- [ ] 协议封装/解封装
- [ ] Socket 创建/发送/接收
- [ ] 握手流程
- [ ] 保活机制
- [ ] 失败回退
- [ ] 自动升级
- [ ] 配置加载

### 兼容性测试

- [ ] 与标准 UDP 节点通信
- [ ] 无权限环境回退
- [ ] 配置禁用
- [ ] 不同版本节点

### 性能测试

- [ ] 吞吐量
- [ ] 延迟
- [ ] CPU 占用
- [ ] 内存占用

### 稳定性测试

- [ ] 长时间运行
- [ ] 网络故障恢复
- [ ] 权限变化
- [ ] 配置热重载
