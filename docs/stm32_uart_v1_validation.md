# STM32 V1、手/自动模式和颜色动作 — 2026-11-07

最新红/绿/蓝扩展验收见本页末尾。前述红色单动作及HEX哈希为前一版本历史。

## 分支和本地修改

Fetch全部分支后确认feature/stm32-uart-v1=e122438为feature/windows-workspace=2a5ab6e的祖先，使用本地fast-forward更新并切换STM32分支，无强制推送、不修改main。
开始前已有4个Keil文件本地修改（uvprojx、uvoptx、自动scatter、RTE_Components.h），已逐字节备份至.local/stm32-before-uart及manifest。保留本地Pack3.0.0、调试器等配置；提交中的uvprojx只基于原始HEAD增加pi_link.c引用，不纳入已有配置差异。

## 实现和范围

- Core/Src/pi_link.c + Core/Inc/pi_link.h：V1帧、CRC16、256槽接收环、目标快照/超时、ACK、500ms STATUS。
- usart.c：现有回调增加USART3分支，先重装单字节DMA再缓冲；USART1 CRSF分支保持原样。MAIN_Task负责启动接收和错误恢复；USART3 IRQ优先级5，与原DMA IRQ一致，匹配FreeRTOS syscall边界。
- stm32f4xx_it.c/.h：增加USART3 IRQ调用HAL_UART_IRQHandler；保留DMA1_Stream1原中断和全部CAN逻辑。
- freertos.c：默认AUTO，UART_RX_Task每10ms复用crsf_is_linked(500)。有CRSF为MANUAL，无CRSF为AUTO，切入AUTO底盘目标只归零一次；电机6仍按原逻辑重发停止。Chassis_Task及CAN Bus-Off代码未修改。
- MAIN_Task仅从BelowNormal改为Normal，保留256栈；解析和阻塞上限10ms的短帧发送在任务上下文中进行，不增加任务。
- TOWER_Task唯一操作SG90：AUTO、CRSF未恢复、新鲜红色有效目标才发-80度；持续红色只触发一次。失效/变色清标志但不自动打开；自动时停止升降、GM6020，保持原塔吊标志含义。手动接管同步最后夹爪角度，原CH4/CH5/CH6（零基）映射不变。
- target有效期=max(0,500-age_ms)，按接收ISR记录的HAL tick计算；任务延迟计入过期。心跳1500ms离线，仅为诊断，不额外改变明确规定的TARGET触发条件。
- 共享环形数据和目标快照用短FreeRTOS临界区；TOWER取快照及下发夹爪前重查CRSF，防止模式任务尚未更新时自动动作。临界区内仅快照、判断和原PWM寄存器写入，不执行阻塞UART或CAN发送。
- 调试器：g_control_mode、原crsf.c链路计数/时间、g_pi_online、g_pi_target、g_auto_red_grab_done、g_pi_frame_errors/g_pi_rx_errors/g_pi_tx_errors。OLED及原视觉算法不变。

## 软件验证

| 验证 | 状态 | 边界 |
|---|---|---|
| Keil ARMCC5.06u7真实rebuild | PASS | 当前本地工程0 Errors、14条原有Warnings，HEX/AXF生成 |
| Keil引用静态检查 | PASS | 63个源文件，include/source全部存在；新增pi_link.c加入工程 |
| 实际C代码主机测试 | PASS | 6项，含V1 CRC/ACK/STATUS、int16边界、分片/粘包/坏帧恢复、valid/color语义、age/心跳/32位tick回绕、半帧/溢出/UART错误/TX失败、实际UART_RX_Task/TOWER_Task分支 |
| Pi原有测试 | PASS | 6项协议/合成视觉测试，无UART硬件操作 |
| 干净提交工程重建 | PASS | 对a4828ce独立checkout使用原始PackID配置真实Keil构建，0 Errors/14 Warnings；与当前本地配置编译的HEX逐字节一致 |

主机测试编译原pi_link.c，并从当前freertos.c提取原任务体，模拟HAL、CAN、PWM、CRSF链路和osDelay。验证的是软件逻辑，不验证真实ISR时序、接线、电机、PWM或RTOS栈余量；GCC仅用于测试，生产构建仍为Keil ARMCC。
命令：python -m unittest discover -s tests/firmware -v；python -m unittest discover -s PI/tests -v；python tools/check_firmware.py；./tools/build_firmware.ps1。
保留已有CRLF文件换行，diff检查使用git -c core.whitespace=trailing-space,space-before-tab,cr-at-eol diff --check。
当前本地构建Code=37568、RO-data=1368、RW-data=700、ZI-data=24316字节（最终确认构建以末尾记录为准）。

## 实机验收（尚未烧录）

| 用户测试 | 状态 | 需要确认 |
|---|---|---|
| 1 Keil编译/链接/HEX | PASS | 已真实构建 |
| 2 手动遥控及CH7塔吊、CH5夹爪、电机6/升降/6020 | UNVERIFIED | 主机分支测试不替代实物回归 |
| 3 断CRSF500ms→AUTO、红色30/-15、CRC/ACK/STATUS/-80度 | UNVERIFIED | 等待用户确认烧录后逐步联调 |
| 4 绿/蓝不触发、valid=0清状态再触发 | UNVERIFIED | 软件逻辑已测试，物理未测 |
| 5 CRSF恢复→MANUAL优先 | UNVERIFIED | 软件分支已测试，实际遥控未测 |
| 真实夹持成功率 | UNVERIFIED | 无夹持传感器，不将闭合命令等同抓取成功 |

## 下一步，仅用户确认后实施

1. 先确认可以烧录本提交，以及ST-Link/C板是否已连接；不会在确认前运行任何下载命令。
2. 用户配合断电核对USART3实物端口与3.3V TTL、交叉TX/RX/共地；不接两板供电脚。Pi物理8 TX→PC11 RX，物理10 RX←PC10 TX，物理6 GND共地。实物接口位置必须核对板卡。
3. 沿用原PA0按键启动门控与PB14逻辑；初始化和上电流程未改，不误认为上电立即开始协议任务。
4. 第一轮保持底盘悬空/机构无干涉，先验手动接管和协议反馈，再按步骤测试夹爪，原SG90_Init仍由main调用。
5. 原SG90角度公式1500+angle*1000/90，-80生成612计数，与驱动注释范围不一致；未改驱动/TIM1。需实物确认机械行程，不在本任务擅改舵机标定。
6. Pi原服务当前可能持续发视觉TARGET；新固件启动后失去CRSF且看见红色（含背景红灯误识别）就可能闭合，首次烧录联调前需按用户批准的步骤协调Pi服务/场景。不得在后台直接发送实物夹爪触发测试帧。

本次不让dx/dy驱动底盘、不自动升降、不改PID/电机映射/视觉算法、不删除本地原件或已有修改。

## 最终构建证据

当前工作目录与.local/stm32-clean-verify干净checkout均真实rebuild成功（11/12秒）。均使用ARMCC5.06u7 build960，0 Errors、14条原有Warnings。
最终Code=37568、RO-data=1368、RW-data=700、ZI-data=24316字节；Flash负载约39636字节，RAM25016字节。
根目录输出firmware/MDK-ARM/DJI C board/DJI C board.hex=110023字节、AXF=1316828字节（含调试信息，非Flash占用）。
HEX SHA256：6e1cad8acce8a6a29b1185745a40b1a3800d8b8d442bdfed3afe7f4bdd67d880。两个构建HEX一致，验证未依赖原有未提交配置差异。日志与临时构建目录均忽略，不提交产物。
编译后核对uvoptx、scatter和RTE文件与任务前备份逐字节一致；本地uvprojx在保留原配置基础上仅增加本次源文件引用。
源码与配置引用检查通过，HAL UART回调每种仅一个定义，USART3 IRQ一个实现。未连接/控制实物执行器，未发送烧录命令；下一步须用户确认后分阶段实测。

## 最新扩展：红闭合、绿张开、蓝前进1秒

- 红色-80°闭合、绿色0°张开，仍由TOWER_Task唯一控制，持续同色只触发一次；蓝色由UART_RX_Task在AUTO分支非阻塞执行，复用CHASSIS_FORWARD_SPEED=0.25m/s。
- 蓝色1000ms到期请求底盘速度归零；持续蓝色不续时/不重触发。目标失效/变色提前停车，CRSF恢复取消自动计时并恢复原手动控制。已有500ms目标新鲜度约束保留，需连续新鲜蓝色目标才能跑满1秒。
- 原Chassis_Task仅增加初始化完成标志，不改变优先级、栈、闭环和CAN代码；初始化完成前不启动蓝色动作。
- 实际C主机测试7项PASS：扩展红/绿单次动作回归，新增蓝色999/1000ms边界、tick回绕、同色不续时、失效/变色停车、再次触发、初始化等待和手动接管。
- Keil ARMCC5.06u7真实rebuild PASS：0 Errors、14条原有Warnings；Code=37760、RO-data=1368、RW-data=704、ZI-data=24320。HEX=110563字节，SHA256=3c404fd4b50550360e073a634e3a1decfd734cbd5a999b338187fe5ec522c195。
- 手动通道映射、SG90驱动、TIM1、底盘PID/CAN参数未改，原本地四个配置修改保留且不提交。本次扩展构建未再创建独立checkout，沿用已验证的同一工程引用和工具链。
- 实物红/绿夹爪、蓝色前进时间/速度/方向、失效制动与遥控接管均UNVERIFIED。本轮未烧录、未远程发送实物触发帧；上轮实板测试也尚未实施。烧录仍须用户确认。
- 烧录后首次测试须协调Pi当前服务/场景：没有CRSF时，已有视觉服务识别到蓝色（含误识别）会触发前进，绿色会张开；先在底盘悬空、机构无干涉条件下逐项测试。
