# 树莓派视觉 UART V1

本文件取代旧通信草案中本阶段的视觉消息部分。不修改 STM32，不发送速度、底盘或夹爪命令。
实际 DJI C 板收发及超时停车：**UNVERIFIED**，待固件实现和实物联调。

帧：`A5 5A | version:u8=1 | type:u8 | seq:u16 | length:u8 | payload | crc:u16`。
所有多字节字段小端；seq 为全部发送消息共享计数，65535 后回到 0。
CRC-16/CCITT-FALSE：poly 0x1021、init 0xFFFF、非反射、xorout 0，覆盖 SOF 至载荷末尾。
标准向量 `123456789` → 0x29B1；接收 CRC 字节为 B1 29。

| type | 名称 | 载荷（按顺序） | 长度 |
|---|---|---|---|
| 01 | HEARTBEAT | uptime_ms:u32 | 4 |
| 02 | TARGET | color:u8 valid:u8 dx:i16 dy:i16 age_ms:u16 | 8 |
| 03 | TARGET_ACK | target_seq:u16 result:u8（0 接收，其他保留） | 3 |
| 04 | STATUS | state:u8 flags:u16 error:u16（语义待固件约定） | 5 |
| 05 | DETECT_REQUEST | 保留，不执行 | 0–255 |
| 06 | QR_RESULT | 保留，不执行 | 0–255 |

用户已规定 TARGET；其余载荷为本次新增的 **Pi V1 接口约定，尚未与固件对接**。
ACK 回显被确认 TARGET 的序号；STATUS 字段只解析和记录，不触发动作。
未知版本、类型、错误固定长度和 CRC 错误均丢弃并重新寻找 SOF。
接收非阻塞，每轮最多读取 1024 字节；发送超时 50 ms，断开后默认 2 s 重连。
串口打开仅表示本地设备可用，不能证明 C 板在线。当前无 ACK 重传，不缓存历史目标。

颜色：1 Red、2 Yellow、3 Blue、4 Green、5 Black、6 Light Blue。
原算法仅支持 Red/Blue/Green，其他颜色选中时始终无目标。
不指定目标色时按颜色编号从小到大选择；同色选原算法最大轮廓。
`dx=cx-290` 右正，`dy=260-cy` 上正，像素单位。原函数已偏移，适配层不再次偏移。
相机必须输出 640×480，否则无效并重试。不重设 HSV 阈值。

TARGET 目标 10 Hz；HEARTBEAT 目标 2 Hz，普通 Linux 调度存在抖动，非硬实时。
age_ms 从 `VideoCapture.read()` 返回后 monotonic 时间戳计算，包含检测及等待发送时间；
**不包含未知的曝光、USB 和驱动排队延迟**，毫秒整数不等于传感器毫秒精度。
缓冲区请求为 1 帧，但设备是否遵从不可保证。超过 500 ms 的观测发送无效目标。
没有目标、采集失败时发送 `color=valid=dx=dy=0`；从不复用旧坐标。
采集独立线程即使阻塞，通信线程仍会在新鲜度期限后发送无效目标。

未来 C 板必须独立校验帧，检测 HEARTBEAT 超时（建议 1500 ms）及 TARGET
失效/过期（建议 500 ms），拒绝无效观测；任何自主运动须由固件单点仲裁并超时归零。
Pi 服务退出后无法发送心跳，不能用 Pi 的清理逻辑代替固件看门狗。
本次没有运动功能，不能在此验证实物超时停车。

## 接线待核验

Pi `/dev/serial0`（当前链接 ttyS0），115200 8N1，无流控、3.3 V TTL。
Pi 物理 8/GPIO14 TX → C 板 USART3 RX；物理 10/GPIO15 RX ← USART3 TX；物理 6 GND 共地。
源码候选 C 板 PC10 TX / PC11 RX，必须按实际板卡接口核验；不要连接 RS232 或 5 V TTL。
USART1 保留 CRSF。接线前断电，不连接两板供电引脚。

## 串口控制台释放记录

本次先保存 `/boot/firmware/cmdline.txt.cpi-uart-v1.bak` 和
`/boot/firmware/config.txt.cpi-uart-v1.bak`，再只删除 cmdline 的
`console=serial0,115200`，mask 并停止 `serial-getty@ttyS0.service`。
原 `enable_uart=1` 保留，未修改任何网络配置。
回滚须在停止 cpi-vision 后恢复备份 cmdline、unmask serial-getty 并重启。
