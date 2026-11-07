# 树莓派视觉 UART V1

本文件取代旧通信草案中本阶段的视觉消息部分。Pi发送视觉观测，不发送底盘速度命令。
2026-11-07新增STM32接收/反馈与自动红色夹爪触发；代码及Keil编译已验证，实际C板收发、模式接管和舵机动作仍 **UNVERIFIED**，未获确认前不烧录。

帧：`A5 5A | version:u8=1 | type:u8 | seq:u16 | length:u8 | payload | crc:u16`。
所有多字节字段小端；seq 为全部发送消息共享计数，65535 后回到 0。
CRC-16/CCITT-FALSE：poly 0x1021、init 0xFFFF、非反射、xorout 0，覆盖 SOF 至载荷末尾。
标准向量 `123456789` → 0x29B1；接收 CRC 字节为 B1 29。

| type | 名称 | 载荷（按顺序） | 长度 |
|---|---|---|---|
| 01 | HEARTBEAT | uptime_ms:u32 | 4 |
| 02 | TARGET | color:u8 valid:u8 dx:i16 dy:i16 age_ms:u16 | 8 |
| 03 | TARGET_ACK | target_seq:u16 result:u8（0 接收，其他保留） | 3 |
| 04 | STATUS | state:u8 flags:u16 error:u16（定义见下文） | 5 |
| 05 | DETECT_REQUEST | 保留，不执行 | 0–255 |
| 06 | QR_RESULT | 保留，不执行 | 0–255 |

用户已规定 TARGET；其余载荷现已在STM32软件端按同一格式实现，硬件联调待验。
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

C板独立校验帧，HEARTBEAT距最近有效心跳达到1500ms标离线；TARGET以age_ms加本地接收后时间超过500ms标失效。任何未来自主运动仍须单点仲裁与独立超时归零。
Pi 服务退出后无法发送心跳，不能用 Pi 的清理逻辑代替固件看门狗。
本阶段不含底盘自动运动，不能在此验证实物自主运动超时停车。

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

## STM32 V1控制与反馈（2026-11-07）

- USART3 115200 8N1，PC10 TX / PC11 RX；DMA1_Stream1 Channel4 normal单字节接收，ISR缓冲到256槽环形区（有效容量255字节）并记录本地HAL tick。MAIN_Task每10ms解析、回复；串口错误由该任务恢复接收。
- C板仅接受HEARTBEAT/TARGET；未知版本/类型/固定长度/CRC错误不更新观测。半帧字节间隔超过100ms重新同步。缓冲溢出或UART错误清残帧并作废已有目标。
- TARGET保留最新观测，valid必须为0/1，有效目标color必须1–6。age_ms>500接收但立即标无效；剩余有效期为500-age_ms。收到valid=0立即失效，过期或连续无更新500ms也失效。ACK result=0表示正确接收观测，不表示执行了夹爪动作或抓取成功；畸形valid/color不回复ACK。
- ACK帧自身seq属于C板统一发送序列，payload.target_seq回显对应Pi TARGET序号。STATUS每500ms发送一次，同样使用C板发送序列。无自动重传；发送失败计入g_pi_tx_errors。
- STATUS state：0=AUTO，1=MANUAL；flags bit0=Pi心跳在线、bit1=视觉目标新鲜且有效、bit2=本轮自动红色闭合命令已下发；其余位0。error=0（本阶段无额外错误编码，详细计数见调试器）。
- g_control_mode默认AUTO；UART_RX_Task复用crsf_is_linked(500)，有效CRSF优先MANUAL，无有效帧则AUTO。模式检查每10ms；MANUAL→AUTO底盘目标仅归零一次，不新增自动底盘速度。CH6（零基）仍为塔吊开关，不是手/自动开关。
- TOWER_Task统一控制SG90：仅AUTO且CRSF未恢复、目标有效且color=1才调用原SG90_SetAngle(-80)，持续红色仅触发一次；目标失效/变色或手动接管重置触发标志，不自动张开夹爪。MANUAL保留原塔吊内CH4（零基、遥控器CH5）三段夹爪逻辑。
- 心跳在线用于状态诊断，本阶段夹爪触发严格按有效新鲜TARGET判断，不额外要求先收到HEARTBEAT；因此新鲜TARGET但无心跳也可能闭合，心跳离线不强制张开夹爪。
- Pi未收到ACK不等于目标无效，收到ACK不等于夹持成功；无夹持传感器。本次不使用dx/dy生成运动指令，未改视觉中心算法。

实测与烧录前检查见 [STM32验收记录](stm32_uart_v1_validation.md)。
