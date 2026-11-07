# C-PI USB CDC

MicroUSB作为USB Full Speed设备，主机为树莓派USB-A；CDC字节流承载原V1二进制帧，不使用USART3 RX。USART1 CRSF保持。
外置电源继续按原接法供电，使用支持数据的USB-A→MicroUSB线；不另接GPIO TX/RX或两板电源引脚。标准C板DBUS为反相USART3_RX，不是本方案接口。

来源：本机已安装STM32Cube_FW_F4_V1.28.3（HAL1.8.5，与原工程相同）。新增USB HAL PCD/PCDEx/LL文件、STM32_USB_Device_Library Core和CDC类，保留原许可证；没有替换现有HAL/CMSIS/FreeRTOS文件。
App/usbd_desc.c/.h、Target/usbd_conf.c基于同包STM324xG_EVAL/USB_Device/CDC_Standalone示例，仅适配FS、板卡GPIO/IRQ/FIFO、保留系统tick优先级、失联清目标和产品名称。
App/usb_cpi.c/.h与Target/usbd_conf.h为本项目适配。CDC类使用静态内存，TX采用持久缓冲；只允许MAIN_Task发送，USB RX回调仅写原接收环。USB reset/suspend/disconnect标记链路错误，旧目标作废。

时钟：12MHz HSE/6*168=336MHz VCO，CPU仍/2=168MHz；PLLQ从4改7，USB48MHz。PA11/PA12为DM/DP，释放未使用的PA11 TIM1_CH4；PE9 TIM1_CH1舵机PWM不变。PA9保留CRSF TX，关闭USB VBUS sensing，不配置OTG ID/主机模式。
FS FIFO：RX128词、EP0 TX64词、EP1 TX64词、EP2 TX16词，合计272词，不超过FS320词。
USB在原KEY启动门控前枚举；KEY未按下时，任务不处理目标/发送STATUS或执行颜色动作。仍须按原PA0 KEY才启动原主程序，不能因已出现ttyACM就认为任务已启动。
IOC已记录引脚和时钟，但当前USB中间件是手工适配，不是CubeMX生成的USB_DEVICE目录；不要直接重新生成来覆盖本目录。原IOC已有任务栈/通道历史配置差异，自动再生成前须审阅diff。

开发枚举ID沿用ST CDC示例0483:5740，产品C-PI CDC，序列号源于芯片UID。实际Linux设备名/serial-by-id待烧录枚举后读取；不假定一定为ttyACM0。
应用层成功提交USB发送不等于对端收到；实际端到端、热插拔、主机识别和动作测试UNVERIFIED，Keil编译通过不能替代硬件验收。
