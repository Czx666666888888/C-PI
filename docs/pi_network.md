# 树莓派有线开发网络

2026-11-05 配置并验证；没有重启或拔插网线测试。

- PC：`enp12s0`，`192.168.123.99/24`。
- 树莓派：`eth0`，`192.168.123.50/24`。
- 复用原有 `Wired connection 1`，UUID `efdd6ae3-10ce-30a3-93f4-90c418902924`，未重复创建连接。
- `connection.autoconnect=yes`，`ipv4.method=manual`，无 IPv4 网关；IPv4 和 IPv6 均 `never-default=yes`。
- Wi-Fi 保持连接，IPv4 地址 `192.168.43.50`；默认路由仍经 `wlan0` 到 `192.168.43.1`。

有线 SSH 验证命令（PC 执行）：

```bash
ssh -i ~/.ssh/id_ed25519_go2 -o BatchMode=yes -b 192.168.123.99 pi@192.168.123.50 \
  'hostname; echo "$SSH_CONNECTION"; ip -4 address show eth0; ip -4 route'
```

已实际成功登录 `raspberrypi`，`SSH_CONNECTION` 两端为 `192.168.123.99` 和 `192.168.123.50`；远端 eth0 地址与路由符合上述记录。自动连接配置已持久化，但重启/拔插后自动恢复为 **UNVERIFIED**。

检查连接配置：

```bash
nmcli -f connection.id,connection.uuid,connection.autoconnect,ipv4.method,ipv4.addresses,ipv4.gateway,ipv4.never-default,ipv6.never-default \
  connection show uuid efdd6ae3-10ce-30a3-93f4-90c418902924
```
