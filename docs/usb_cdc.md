# 树莓派 USB ↔ C 板 MicroUSB：CDC V1

## 接线

树莓派普通USB-A主机口 → 支持数据传输的USB-A到MicroUSB线 → C板MicroUSB设备口。两板沿用各自正常电源，C板机构使用已有外置电源；不再接GPIO UART、不接两板3.3V/5V引脚。USB线本身已有数据地与VBUS，不额外用GPIO电源脚并联供电。
BOOT保持正常Flash启动；MicroUSB在本方案中是CDC通信，不是DFU下载模式。新固件首次仍按原ST-Link/SWD流程烧录，不能把未实现CDC的旧固件通过插线变成虚拟串口。
标准C板USB电路和供电见官方手册，第8–9页：
https://rm-static.djicdn.com/tem/35228/RoboMaster%20%20%E5%BC%80%E5%8F%91%E6%9D%BF%20C%20%E5%9E%8B%E7%94%A8%E6%88%B7%E6%89%8B%E5%86%8C.pdf
标准C板USART3 RX通过DBUS反相电路，PC10 TX未在正常UART接口引出；旧USART3接线表仅代表MCU原始信号，不应照着接DBUS。USB方案避免这些裸信号接线。

## 软件实现

STM32 USB FS CDC承载原V1 A5 5A帧、CRC16、HEARTBEAT/TARGET/TARGET_ACK/STATUS，端序、序号、颜色动作均不变。当前默认传输已切USB，MAIN_Task不再启动USART3 DMA；遗留UART3初始化/回调保留，不作为并行输入。
PA11/PA12为USB DM/DP；释放未使用的PA11 TIM1_CH4。PLLQ4→7产生48MHz，CPU仍168MHz，APB/CAN/USART/PWM频率不变。USB不占用PA9/PA10的VBUS/ID功能，USART1 CRSF完整保留，SG90 PE9/TIM1_CH1未变。
USB在KEY门控前枚举，原KEY未按下时任务不处理目标或发STATUS，不执行颜色动作。按KEY后才启动原流程。
来源为本机已安装STM32CubeF4 V1.28.3，与现有HAL同为1.8.5；仅补USB所需HAL文件和ST USB Device Core/CDC，原HAL/CMSIS/FreeRTOS未升级。来源及适配说明见firmware/USB_DEVICE/README.md，原许可证保留。
CDC RX回调只缓冲字节并重装OUT接收，MAIN_Task解析/发送；TX使用静态持久缓冲，异步完成前不覆盖，忙时最多让出任务等待10ms。单CDC类静态分配，不在ISR使用malloc。
USB复位/挂起/断开会作废旧目标；蓝色目标失效后按已有10ms任务循环请求停车。实际断线制动和热插拔恢复待硬件验证，不能只凭软件推定通过。

## 树莓派端（烧录后执行）

先检查设备，不盲猜名字：

```bash
lsusb
ls -l /dev/ttyACM* /dev/serial/by-id/
```

开发枚举VID:PID沿用ST CDC示例0483:5740，产品C-PI CDC，序列号源于芯片UID；实际设备可能是ttyACM0或其他编号。优先读取并使用这块C板的serial/by-id稳定路径。
PI/config/usb.json提供暂用/dev/ttyACM0的配置模板；未枚举前不要直接用其启动，更不能宣称已连通。实际命令例如：

```bash
# 只有停止原相机服务、确认串口与相机无重复进程后才启动。
/home/pi/opencv_test/.venv/bin/python /home/pi/cpi-vision/main.py --mode combined --serial-device /dev/serial/by-id/实际C板路径
```

现有pyserial已能读写CDC，无需换协议或改视觉算法；115200作为CDC线路编码参数保留，不代表USB总线实际波特率。旧cpi-vision服务仍配置为serial0，本次未改远端unit/配置、未部署或启动新进程。实际设备确认后再备份并调整服务启动参数。
默认不指定目标色时Pi按颜色编号选目标：红优先于蓝再绿；逐色测试时只放一种物块，避免误把多物块场景理解为三种动作同时执行。
相机预览窗口PI/preview_material.py本身不打开串口、不发送动作；需要实时观察并触发动作时须明确安排单一相机采集进程，不能与combined服务同时抢占相机。

## 验收边界

2026-11-07：真实Keil ARMCC5.06u7 rebuild PASS，0 Errors、14条原有Warnings；73个源文件引用完整。Code47592、RO-data1368、RW-data956、ZI-data27228字节；HEX138725字节。
10项固件主机测试PASS（原协议/模式/颜色动作7项、USB适配3项：异步TX缓冲生命周期与超时/断开、CDC RX及line-coding、静态分配）；Pi原6项测试PASS。
这些测试不模拟真实USB电气/主机枚举/PCD中断。实际Linux ttyACM识别、双向ACK/STATUS、插拔、相机联动和机构动作全部UNVERIFIED。尚未烧录，烧录仍须用户确认。

当前HEX SHA256：c0bd35ccb75cae87b6b80a672a8dc357a8d3d982477834b9ca89f0bf0c06df55。

独立干净checkout 82fbdb9也已真实Keil重建PASS（0 Errors/14 Warnings），HEX与当前工作目录构建逐字节一致，验证不依赖原本地未提交Keil配置。

## 用户烧录后的首轮检查（2026-11-07）

用户报告已烧录并已换MicroUSB数据线/树莓派USB口、按RST，未按KEY。尚未通过SWD读取Flash核对，不把用户报告等同于已验证运行映像。
- USB枚举检查FAIL：树莓派lsusb仅有根Hub和Alcorlink相机，无0483:5740；/dev/ttyACM*和/dev/serial/by-id均不存在。内核日志也未见C板新连接事件。原因未定，下一步检查探针/启动状态，不据此认定驱动或固件已正常。
- USB双向协议、ACK/STATUS、颜色执行器联动均UNVERIFIED：没有设备节点，未发送任何USB测试帧或机构指令。
- 经用户允许暂停原cpi-vision后，Windows只读颜色预览PASS：640×480 MJPG，90.00s有效流，显示1344帧/解码1353帧，14.92FPS，主动preview_skipped=9，decode_failures=0，日志read_failures=0；曾观察Red=(-28,54)。不以此声称全颜色识别/中心精度验收。
- 预览正常结束，ssh_reaped=True、receiver_alive=False；确认无预览摄像头占用后恢复原服务active/running，PID2299。原服务仍使用serial0，未改为USB设备，也未另开第二相机采集。
- 本轮没有改固件或重新烧录。Windows当前未识别到ST-Link，已请用户接回SWD探针，准备检查程序启动/USB初始化位置。
