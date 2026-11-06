# 树莓派源码迁移

`legacy/` 是从树莓派 `/home/pi/opencv_test/*.py` 原样复制的 10 个源文件。
源机：`pi@192.168.123.50`；迁移日期：2026-11-05。
未复制 `.venv`、缓存；归档阶段未运行源脚本，未改变树莓派原目录。后续检测函数实测见下文。
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

- 摄像头已由用户插入，实物采集、原物料检测函数和 PC 实时预览链路已运行。当前仅确认此 USB 摄像头映射为 `/dev/video0`，不假定存在第二台摄像头。识别准确率与长期稳定性仍 **UNVERIFIED**。
- `/dev/serial0 -> ttyS0`，`enable_uart=1`；内核命令行含 `console=ttyS0,115200`，`serial-getty@ttyS0.service` 为 active。正式串口接入前必须处理控制台占用并核对接线。本轮未修改启动配置、串口或固件。
- 后续先独立实现并验证新通信帧检验及超时停车，再迁移视觉模块、统一颜色编号与坐标约定。当前未实现新通信链路；帧检验、超时停车及实机联调均 **UNVERIFIED**。

## 本轮验证

- `ast.parse` 逐文件解析 `legacy/*.py`：10/10 PASS；不 import、不打开串口或摄像头。
- 源机 `sha256sum ~/opencv_test/*.py` 与本地清单：10/10 一致。
- 未编译/烧录 C 板，未发送运动指令，未运行视觉或旧串口脚本。

## 摄像头接入后的物料检测预览（2026-11-05）

用户随后插入 USB 摄像头，新增 `preview_material.py`：通过 SSH 在树莓派调用原始
`physical_detect.get_color_blocks_positions()`，编码带标记 JPEG 传回 PC 显示。
不启动旧主循环，不打开串口；原检测阈值和原始文件保持不变。
使用 640×480 MJPG，确保原参考点 `(290, 260)` 在图像内；它仍不是标定坐标。
稳定设备路径为 `/dev/v4l/by-id/usb-Alcorlink_Corp._USB_2.0_Camera-video-index0`。

PC 运行（需要 OpenCV、NumPy 和已经配置的 SSH 密钥）：

```bash
python3 raspberry_pi/preview_material.py
```

窗口聚焦后按 Q 或 Esc 停止。GTK 后端不一定提供窗口可见性属性，优先使用键盘退出。
接收端仅保留最新帧；日志 `preview_skipped` 是 PC 主动略过的预览帧，不能等同于摄像头丢帧。
持续 12 秒没有收到新图像会退出；每 5 秒输出采集/解码计数和速度。

已运行的验证：

- `python3 -m py_compile raspberry_pi/preview_material.py`：PASS。
- `python3 raspberry_pi/preview_material.py --headless --frames 90 --snapshot /tmp/cpi_material_preflight.jpg`：90 帧采集、传输、解码完成，预览略过 0；Pi 约 17 FPS，读取失败 0；快照已查看。
- 预检退出后 `fuser /dev/video0` 无占用，随后成功启动持续 GUI 预览。
- PC `xwininfo -root -tree` 确认 960×720 检测窗口已创建；持续日志显示 Pi 约 18 FPS，读取失败 0，并返回 Green 像素位置。

以上验证了实物摄像头采集、检测函数执行和 PC 显示链路。物料分类准确率、全部颜色识别效果
尚待用户放置物料观察，不能由日志中的 Green 结果直接认定识别正确。没有串口或运动测试。

持续 GUI 测试随后在 62.7 秒、1063 帧时中断：摄像头读取失败，内核日志确认
23:13:54 发生 USB disconnect，伴随地址请求错误 -71，23:13:55 重新枚举。
这是设备断连的观测，原因尚不明确；不能将本轮视为长时间稳定性通过。设备恢复后重新启动预览。
第二次运行显示 313 帧、21.7 秒后再次因设备断开结束；随后设备重新出现。
只读检查 `vcgencmd get_throttled` 为 `0x0`，未报告欠压/降频标志；这不能排除 USB 接头或供电问题。
内核日志中摄像头及 USB 键盘均有断开后重新枚举记录。用户说明曾把镜头放到地上、画面变黑；这不构成主动拔插的确认，也不足以确定 USB 断连根因。

## 收工状态与下次任务（2026-11-05 更新）

- 另一次恢复后的运行正常退出，显示 474 帧、30.0 秒，日志中出现 Red/Green/Blue 三类返回值；这些返回值不等于分类准确率验收。
- 全黑图验证：在 Pi 用 `np.zeros((480,640,3), dtype=np.uint8)` 生成全黑帧，重复 100 次调用原检测函数；100 次均返回 `{}`，正常完成。后续实时日志也出现 `positions={}` 同时帧数持续增长、读取失败 0。因此没有“无物料/画面黑就停止”的程序逻辑。
- 设备中断与黑画面应分开：前两次失败有 USB disconnect 和重新枚举证据，根因未确定。移动导致线缆接触变化、USB 供电或设备异常都是待排查假设，不能写成已确认结论。
- 最后只读复查：没有本轮远端 `python -u -` 检测进程，`fuser /dev/video0` 无占用；预览已结束、摄像头已释放。`vcgencmd get_throttled` 仍为 `0x0`；检查到的最新内核日志未出现晚于 23:15:34 的 USB 事件。
- 未开启串口、未发送底盘/执行机构指令，C 板固件未改动。

用户计划 2026-11-06 继续：

1. 先固定 USB 连接并确认持续预览，再采集有标签的物料/背景样本，覆盖明暗、距离与姿态；建立识别效果基线。
2. 优化物料识别：背景误检、HSV 阈值、轮廓筛选和结果稳定性；统一比赛六色编号。参考点与像素坐标先明确，不将其冒充已标定物理坐标。
3. 编写上位机通信部分：先定稿 HELLO、HEARTBEAT、SET_VELOCITY、STATUS 的长度、序号、单位、坐标系、CRC 与超时契约，再实现解析和离线验证。
4. 串口实连前处理 `ttyS0` 控制台占用、核对 USART3 实物接口；自主接入前落实单点控制权仲裁。帧检验及超时停车验证不可省略，实机未测继续标记 **UNVERIFIED**。

## V1 联合服务

新入口 `main.py`，独立部署到 `/home/pi/cpi-vision`；仅 Python 解释器复用
`/home/pi/opencv_test/.venv/bin/python`，检测源码来自新目录 `legacy/physical_detect.py`。
旧工程不删除、不改动。依赖 `opencv-python numpy pyserial`，无 ROS2 或 GUI 依赖。
协议见 [UART V1](../docs/uart_v1.md)。默认无窗口；可选 `--preview` 仅用于本机桌面。

PC 同步（不复制 venv/缓存，不使用 rsync --delete）：

```bash
bash raspberry_pi/scripts/deploy.sh
```

树莓派手动测试：

```bash
cd /home/pi/cpi-vision
/home/pi/opencv_test/.venv/bin/python main.py --mode simulate --serial-device /dev/pts/3 --duration 10
/home/pi/opencv_test/.venv/bin/python main.py --mode vision --duration 30
/home/pi/opencv_test/.venv/bin/python main.py --mode combined --target-color Red
/home/pi/opencv_test/.venv/bin/python scripts/mock_uart_test.py
```

`simulate` 不打开相机；`vision` 不打开串口；`combined` 两者同时运行。
配置集中于 `config/default.json`，支持 `--config`、`--camera`、`--serial-device`、
`--target-color` 覆盖。终端与 journal 每 5 秒输出视觉结果、目标、发送计数和串口状态，
收到 ACK/STATUS 当次记录。不要同时启动手动程序和服务争抢相机。
SIGINT/SIGTERM 正常释放资源；底层相机若阻塞，systemd 会在 8 秒内终止整个进程组。

先手动验证，再安装服务：

```bash
bash /home/pi/cpi-vision/scripts/install_service.sh
sudo systemctl start cpi-vision
sudo systemctl stop cpi-vision
sudo systemctl restart cpi-vision
systemctl status cpi-vision --no-pager
journalctl -u cpi-vision -f
sudo systemctl disable --now cpi-vision
```

未来连接 C 板手动测试前先 `sudo systemctl stop cpi-vision`，再运行：

```bash
/home/pi/opencv_test/.venv/bin/python /home/pi/cpi-vision/main.py --mode combined --serial-device /dev/serial0 --target-color Red
```

这仅发送视觉观测，不控制运动；必须在固件实现匹配协议后联调。
