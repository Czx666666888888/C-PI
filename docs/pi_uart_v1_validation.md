# Pi UART V1 部署验收（2026-11-06）

实现提交：`508c044`，分支 `feature/pi-uart-v1`，已 Push 并用 `git ls-remote` 核对。
本记录仅涉及 Pi，`firmware/` 无修改。C 板未接入，无运动指令。

## 当前部署

- SSH `pi@192.168.123.50` 重启后可连接，未修改网络配置。
- 新入口 `/home/pi/cpi-vision/main.py --mode combined`。
- 解释器 `/home/pi/opencv_test/.venv/bin/python`（只复用环境，源码不依赖旧目录）。
- 相机 by-id Alcorlink index0，实际输出 640×480；原物块算法未改。
- `/dev/serial0 -> ttyS0`，115200 8N1，串口控制台已释放；串口打开不等于对端在线。
- `cpi-vision.service` enabled/active，正常重启后自动运行。
- 最后检查仅服务 PID 1496 占用 `/dev/video0` 与 `/dev/ttyS0`。PID 后续会变化。
- 主程序、检测原文件和 unit 的 PC/Pi SHA256 均一致。
- Pi 时钟当次较 PC 约慢 22 小时；未改时间配置，age 使用 monotonic。

## 实际执行及结果

| 项目 / 命令 | 结果 | 证据与边界 |
|---|---|---|
| PC `python3 -m compileall -q raspberry_pi/main.py raspberry_pi/communication raspberry_pi/vision raspberry_pi/scripts raspberry_pi/tests` | PASS | 新代码语法 |
| PC `python3 -m unittest discover -s raspberry_pi/tests -p test_protocol.py -v` | PASS | 4 tests；CRC 0x29B1、分片/粘包、坏帧恢复、int16 极值、超时失效 |
| Pi venv `python -m unittest discover -s tests -v` | PASS | 6 tests，包括原算法合成 RGB 图 (30,-15)、输入图不改、空图、固定选择、采集故障清空与重新打开 |
| Pi venv `python scripts/mock_uart_test.py` | PASS | PTY 实际进程收发；最终 TARGET 9.796 Hz、HEARTBEAT 1.999 Hz；正负坐标、ACK/STATUS、断开存活、重新连接 |
| Pi venv `python main.py --mode vision --duration 20` | PASS | 真实相机日志至少 312 帧，无目标持续 `{}`，正常释放 |
| Pi venv `python main.py --mode combined --duration 10` | PASS | UART 本地打开及周期写入，相机持续；无 C 板 ACK，RX=0 |
| Pi venv `python main.py --mode combined --serial-device /dev/cpi-missing --duration 10` | PASS | 串口每 2s 重连，真实相机采集持续，日志 88 帧 |
| Pi venv `python main.py --mode combined --camera /dev/cpi-missing --duration 6` | PASS | 每 2s 重开相机，UART 仍发送 valid=0；约 5s TARGET=50、HEARTBEAT=10 |
| `systemctl stop/start/restart cpi-vision`、`fuser` | PASS | 停止后无相机/串口占用，手动恢复成功 |
| `sudo reboot` 后 SSH、`/proc/cmdline`、`systemctl is-active/is-enabled` | PASS | SSH 恢复，串口 console 已移除，服务自动启动，重启后初始 NRestarts=0 |
| `systemctl kill --signal=SIGKILL --kill-whom=main cpi-vision` | PASS | 3s 后自动恢复 active/running，NRestarts=1；故意故障注入，不代表自发崩溃 |
| 真实红绿蓝物料识别/偏差验收 | UNVERIFIED | 用户明确尚未测试；合成图不替代实物 |
| 真实 USB 拔插重新枚举恢复及长期稳定性 | UNVERIFIED | 已测软件故障注入/缺失设备；未进行物理拔插 |
| DJI C 板实际收发、ACK/STATUS 字段及超时停车 | UNVERIFIED | 当前未接板、未修改固件；字段需后续固件匹配 |

PC 直接运行 mock 曾因无 pyserial 未通过；随后使用 Pi 原 venv 测试通过，未在 PC 安装额外包。
真实 UART 首轮暴露出将正常排队误判为故障，已将队列限制改为 64 字节并完成真实 UART 与 PTY 复测。
发送频率为普通 Linux 尽力调度，非硬实时；发送计数表示驱动接受写入，不是物理波形测量。

## 后续人工验收

1. 服务运行时依次放置红/绿/蓝物料，通过 `journalctl -u cpi-vision -f` 核对实际颜色和参考点方向。
2. 经用户配合进行 USB 拔插，观察 invalid → reconnect → 新目标，排查原 USB 断开原因。
3. C 板固件实现匹配的 [V1](uart_v1.md) 后，断电核对 USART3 实物接口，再交叉连接 TX/RX 与共地。
4. 验证实物 CRC/序号/ACK/STATUS；未来运动开发必须另做控制权仲裁及固件超时停车测试。

运行、日志、停用命令与未来联调启动命令见 [Pi README](../raspberry_pi/README.md#v1-联合服务)。
