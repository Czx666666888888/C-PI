# Windows 迁移验收 — 2026-11-07

## 来源与文件保护

工作区 D:/26C-PI，原先无 .git，PI 空。Git 2.53.0.windows.1。
远程 main=8d12640a92e723f8916fefff4c6327b3fa6c776c；pi-source-migration=15f6d00135a5dab8dcf266adf17db8ad216168a7；pi-uart-v1=e12243803817ed69c990a9dda90cf62da2938732。
三者为连续祖先关系，当前 feature/windows-workspace 基于 e122438；本地 feature/stm32-uart-v1 也预留在该安全基线上。未合并 main、未 force push。
完整历史下载到 .migration-remote 后将 Git 元数据关联根目录；原件 DJI C board(1)/DJI C board 保留，未删除、移动、覆盖。
远程固件 198 文件，除远程独有 README 外均在本地存在；字节差异 113 个，全部仅 CRLF/LF，实际源码差异 0。工作固件保持远程原始字节避免换行噪声，原件保留本地字节。
raspberry_pi → PI 是同一历史中目录迁移，legacy 10 个文件保持字节和 SHA256SUMS，不复制虚拟环境。
仅移除失效 include ../Lib/Telescopic/Inc；不创建空工程、不改算法、不新增固件通信、不改电机逻辑。
停止跟踪旧 startup .lst 生成物，本地文件仍保留；scatter 文件作为已有链接配置保留。

## Keil 验收

注册表 MDK 5.32，UV4 位于本机 D:/Keil_v5/UV4/UV4.exe；ARMCC 5.06u7 build 960（与工程匹配）。
工程声明 Keil.STM32F4xx_DFP.2.8.0，本机实际安装 D:/Keil_v5/ARM/Packs/Keil/STM32F4xx_DFP/3.0.0；未升级/降级或改工程 PackID。
STM32CubeMX 发现于 D:/STM32Cube/STM32CubeMX/STM32CubeMX.exe。CubeProgrammer 在常见安装目录及 PATH 未检出，不影响 Keil 构建/下载；并非断言全磁盘无安装。
ST-Link DLL、USBDriver 安装包和 STM32F4xx_1024.FLM 存在；实物驱动安装/绑定状态 UNVERIFIED。

实际命令（以及 tools/build_firmware.ps1 再次重建均通过）：

```powershell
& 'D:/Keil_v5/UV4/UV4.exe' -r 'D:/26C-PI/firmware/MDK-ARM/DJI C board.uvprojx' -j0 -o 'D:/26C-PI/.local/keil-build.log'
```

PASS：0 Error(s)、14 Warning(s)，12–14 秒。Code 34864、RO-data 1368、RW-data 664、ZI-data 22232 字节；Flash 负载约 36896 字节，RAM 22896 字节。
HEX 102426 字节；第二次 AXF 1290932 字节（含调试信息，不能作为 Flash 占用）。文件位于 firmware/MDK-ARM/DJI C board/。
原有警告为末尾无换行及未引用符号，未做运动控制重构。
62 个源文件及所有 include 路径存在；宏 USE_HAL_DRIVER/STM32F407xx；原启动文件、HAL、CMSIS、FreeRTOS 与 Lib 完整。
Flash 0x08000000/1 MiB，RAM 112+16 KiB，useFile=0 自动 scatter；未擅自引入 CCM RAM。
Debug/Utilities ST-Link，SW-DP，STM32F4xx_1024.FLM 起始 0x08000000、大小 0x100000；软件配置已检查。GUI 交互、Reset 策略适配实板、SWD 接线及烧录 UNVERIFIED。本次无下载命令。

## Pi 已有实现与验证边界

实际源码包含：UART 115200、8N1（pyserial bytesize=8/parity=N/stopbits=1）、2 Hz HEARTBEAT、10 Hz TARGET、接收 TARGET_ACK/STATUS、CRC16、断线重连、无目标或陈旧目标 valid=0、systemd unit。
vision/detector.py 复用原 physical_detect；固定 (290,260)，输出已是相对偏差，不再次偏移。原二维码、相机、颜色及串口脚本在 legacy；旧脚本存在未定义依赖，不能宣称全部可运行。
Windows 本次 unittest discover -s PI/tests -v：6 PASS（协议与合成视觉）；同步工具 unittest discover -s tools -p test_pi_sync.py -v：4 PASS；静态检查 PASS。
旧 Pi 实机证据见 pi_uart_v1_validation.md，仅历史记录，非当前硬件验证。POSIX PTY 集成脚本不能在 Windows 直接运行，本次未运行。

## 网络、部署和人工步骤

Windows 有线网卡显示 Media disconnected；WLAN 为 10.60.137.68/16，未见 192.168.123.0/24 网卡。
SSH BatchMode / StrictHostKeyChecking=yes 对 pi@192.168.123.50 探测失败：Connection closed；没有确认身份或认证成功，不据此认定 Pi 在线。
连接树莓派电源和有线网线，在对应以太网网卡设置 Windows IPv4 192.168.123.1、掩码 255.255.255.0；直连网卡不设网关/DNS，保留 Wi-Fi 上网。避免与已有设备地址冲突。

```powershell
ping 192.168.123.50
Test-NetConnection 192.168.123.50 -Port 22
ssh pi@192.168.123.50
# 首次核对指纹，输入凭据；随后配置 SSH 密钥/agent，禁止提交私钥
./tools/deploy_pi.ps1
./tools/deploy_pi.ps1 -Apply
./tools/pull_pi.ps1
./tools/pull_pi.ps1 -Apply
ssh pi@192.168.123.50 'systemctl status cpi-vision --no-pager'
ssh pi@192.168.123.50 'journalctl -u cpi-vision -n 50 --no-pager'
ssh pi@192.168.123.50 'ls -l /dev/v4l/by-id /dev/serial0'
```

默认远端 /home/pi/cpi-vision，保留原 /home/pi/opencv_test。tools 脚本走 Windows OpenSSH 的 SSH stdin 文件传输，不要求 WSL/rsync；基于 SHA256 比对和写入前复查。默认预检查、不同内容首同步拒绝覆盖，回拉保护未提交 PI。无删除，无自动服务操作。
当前同步实机 UNVERIFIED；单元测试只验证本地算法，不能替代 SSH 端到端。部署前审阅远端独立修改，出现冲突先人工合并。服务重启和相机/UART 测试应在 Pi 执行。

## 下一阶段 USART3

Core/Src/usart.c：115200 8N1，PC10 TX、PC11 RX，AF7；DMA1_Stream1 Channel4 RX、normal 模式、very-high DMA 优先级；无 TX DMA。
Core/Src/dma.c 使能 Stream1 IRQ，NVIC 优先级5；stm32f4xx_it.c 已调用 HAL_DMA_IRQHandler，但没有 USART3_IRQHandler/USART3 IRQ 使能；当前未启动 USART3 DMA 接收。
HAL_UART_RxCpltCallback 和 ErrorCallback 在 usart.c 仅处理 USART1 CRSF；USART1 420000、PA9 TX/PB7 RX、TX DMA2_Stream7，须完整保留。
后续可在现有回调加 huart3 分支，新增 Lib 下 V1 解析模块，按需加 USART3 IRQ/IDLE 接收与 DMA 重启；新增文件需加入 uvprojx。不能新增重复全局 HAL 回调。
freertos.c 的 StartTask04 名为 UART_RX_Task，但实际负责 CRSF 并周期写 chassis_set_target；建议新增独立消息处理任务，通过消息/共享观测上报，暂不写底盘目标。启用 FreeRTOS ISR API 前核对 FreeRTOSConfig 中 syscall 优先级。
静态未见 PC10/PC11 或 DMA1_Stream1 的第二所有者；实物接口/接线仍 UNVERIFIED。USART6 另有初始化，勿误接 USART1。
后续协议 HEARTBEAT/TARGET(color,valid,dx,dy,age_ms) 与 ACK/STATUS 遵循 uart_v1.md；先验证协议、超时、错误帧与控制权，再另行申请运动测试。
