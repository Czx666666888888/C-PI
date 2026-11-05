# 树莓派源码迁移

`legacy/` 是从树莓派 `/home/pi/opencv_test/*.py` 原样复制的 10 个源文件。
源机：`pi@192.168.123.50`；迁移日期：2026-11-05。
未复制 `.venv`、缓存；未运行源脚本，未改变树莓派原目录。
`legacy/SHA256SUMS` 保存逐文件 SHA-256，已与源机输出核对一致。

## 原环境

- Python 3.13.5
- numpy 2.4.4
- opencv-python 4.13.0.92
- pyserial 3.5

这些版本来自原虚拟环境的 `pip list --format=freeze`，是环境记录，不代表在 PC 安装或验证过。

## 静态审阅

- `q2_detect.py`：二维码检测类，可作为后续模块化迁移的起点。
- `physical_detect.py` / `circle_detect.py`：物块与圆环识别。只处理红绿蓝；像素参考点写死为 `(290, 260)`；其中物块脚本的测试采集设置为 320×240，参考点超出图像。像素坐标未标定为米，不能直接作为底盘速度或定位值。圆环函数还直接调用 GUI 显示。
- `sequence_test.py`：视觉/串口流程原型，颜色编号 `1=Red, 2=Green, 3=Blue` 与项目规则不一致（规则为红1、黄2、蓝3、绿4、黑5、浅蓝6）。
- `serial_communication.py` / `sequence_test.py`：旧帧 `0x66 ... 0x77`，默认 9600 baud；接收只提取 checksum，不验证；缺少序号、CRC、心跳和自主超时停车契约。不是当前通信草案的实现。
- `TransportCAR.py`：主循环引用未定义的 `unit`，并包含未定义的 `ser`、`send`、`kalman_filter` 等依赖；语法可解析不等于能运行。
- `color_detect.py`、`q2_detector.py`、`test_camera_2Dcode.py` 在模块顶层打开摄像头；`test.py` 顶层处理图像并写文件。不要通过批量 import 做验证。
- C 板 USART3 源码为 115200、8N1，PC10 TX / PC11 RX；实物接口仍待核对。USART1 保留 CRSF。现有 `StartTask04` 每 10 ms 写底盘目标，自主模式接入前必须统一仲裁。

## 硬件状态与下一步

- 用户确认摄像头未插入；当前未枚举 `/dev/video0`、`/dev/video2` 属预期状态。插入后重新确认设备编号、预览和帧率。采集与识别为 **UNVERIFIED**。
- `/dev/serial0 -> ttyS0`，`enable_uart=1`；内核命令行含 `console=ttyS0,115200`，`serial-getty@ttyS0.service` 为 active。正式串口接入前必须处理控制台占用并核对接线。本轮未修改启动配置、串口或固件。
- 后续先独立实现并验证新通信帧检验及超时停车，再迁移视觉模块、统一颜色编号与坐标约定。当前未实现新通信链路；帧检验、超时停车及实机联调均 **UNVERIFIED**。

## 本轮验证

- `ast.parse` 逐文件解析 `legacy/*.py`：10/10 PASS；不 import、不打开串口或摄像头。
- 源机 `sha256sum ~/opencv_test/*.py` 与本地清单：10/10 一致。
- 未编译/烧录 C 板，未发送运动指令，未运行视觉或旧串口脚本。
