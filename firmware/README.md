# DJI C 板 Keil 固件

入口 `MDK-ARM/DJI C board.uvprojx`，保留原工程，未重新生成 CubeMX 工程。
STM32F407IGHx；ARMCC 5.06 update 7 build 960；宏 USE_HAL_DRIVER、STM32F407xx。CMSIS/HAL、FreeRTOS RVDS/ARM_CM4F、heap_4、CAN/底盘/CRSF/BMI088/升降驱动齐全，62 个源文件及 include 引用检查通过。
工程声明 DFP 2.8.0，本机已安装 3.0.0；本次未安装或切换版本，现有环境真实 rebuild 成功。
唯一手工工程配置改动为移除不存在且无源文件引用的 ../Lib/Telescopic/Inc。
启动文件 MDK-ARM/startup_stm32f407xx.s；CubeMX 为 DJI C board.ioc。

根目录执行 `./tools/build_firmware.ps1`，输出 MDK-ARM/DJI C board/DJI C board.axf 和 .hex；日志根目录 .local/。
2026-11-07：0 Errors、14 Warnings。Code=34864、RO-data=1368、RW-data=664、ZI-data=22232 字节。
警告为原有末尾换行及未引用函数/变量；未为消警告改运动控制。
Flash 0x08000000、1 MiB；RAM 0x20000000/112 KiB + 0x2001C000/16 KiB。useFile=0，链接按工程配置自动生成 scatter，保留原内存布局。

Debug/Utilities 使用 ST-LINKIII-KEIL_SWO.dll；uvoptx 记录 ARM CoreSight SW-DP；下载算法 STM32F4xx_1024.FLM，Flash 起点 0x08000000、大小 0x100000。本机 FLM、ST-Link DLL 与驱动安装包存在。
实物驱动绑定、GUI 交互、SWD 接线、Reset 策略及下载均 UNVERIFIED。连接后从 Options for Target → Debug → ST-Link Settings 核对 SWD、复位与 Flash Download。本次未烧录，会驱动电机的下载须用户另行确认。
USART3资源审阅基线见 ../docs/windows_migration.md；当前已新增Pi V1解析和反馈，实现及实测边界见 ../docs/stm32_uart_v1_validation.md。

## STM32 UART V1增量

新增Core/Src/pi_link.c、Core/Inc/pi_link.h，工程现为63个源文件。USART3 DMA启动与错误恢复由现有MAIN_Task执行，反馈发送仅在任务中；USART1 CRSF保持原实现。MAIN_Task调整Normal、256栈，其余线程配置不变。
UART_RX_Task每10ms更新g_control_mode；TOWER_Task唯一负责夹爪手/自动命令。自动红色有效目标闭合-80度、绿色张开0度、蓝色触发0.25m/s前进最多1秒，同色不重复触发；目标失效/变色停车，手动优先。见docs/uart_v1.md。
调试器观察g_control_mode、g_pi_online、g_pi_target（color/valid/dx/dy/age_ms/seq/last_rx_tick）、g_auto_red_grab_done与g_pi_*_errors；CRSF可观察原crsf.c中的s_last_frame_tick/s_frame_count。原OLED显示保持。
原SG90_SetAngle实际公式为1500+angle*1000/90，-80对应612个计数；与驱动注释所说的1000–2000范围不一致。本次按要求复用、不修改TIM1或驱动，机械行程和真实闭合需用户确认烧录后验证，不将命令下发描述为抓取成功。

上文Code/RAM数值为迁移基线；当前V1固件最终构建与输出SHA256以docs/stm32_uart_v1_validation.md为准。

## 当前传输：USB CDC

USB-A（树莓派）到MicroUSB（C板），V1帧格式不变，MAIN_Task默认不再启动USART3 RX。USB固件真实编译0 Errors/14 Warnings，原三色动作及CRSF不变，硬件尚未验证。接线、时钟/引脚适配和测试边界见../docs/usb_cdc.md与USB_DEVICE/README.md。
