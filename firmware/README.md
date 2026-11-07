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
USART3 后续修改位置和资源审阅见 ../docs/windows_migration.md，目前只有初始化，没有 Pi V1 解析。
