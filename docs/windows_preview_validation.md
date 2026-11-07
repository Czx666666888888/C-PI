# Windows SSH 与真实摄像头验收（2026-11-07）

## 网络与认证

PASS：物理“以太网”Realtek PCIe GbE、ifIndex10，192.168.123.99/24，AddressState=Preferred，无默认网关；到 192.168.123.50 的实际路由为该网卡。Ping 2/2，2–3ms。WLAN 10.60.137.68/16、网关10.60.0.1保持不变。
早先未配置有线 IP 时，TCP22 探测经 Meta 代理网卡返回成功，但 SSH 握手失败，不能视为 Pi 连通证据。
用户完成首次登录/主机信任，随后安装本机专用公钥；BatchMode=yes、StrictHostKeyChecking=yes 真实登录 PASS，hostname=raspberrypi、whoami=pi、pwd=/home/pi。未读取密码或私钥，专用密钥只在 Windows 用户 .ssh 下，不在仓库。
主机公开 ED25519 指纹 SHA256:5MkeAC8TW2U6+V7GhkdVhKQdK0jyGSbK9KotmDTre5w。已记录 known_hosts 并严格校验；没有独立读取 Pi 本地显示器的指纹，本机首次信任由用户完成。
SSH 别名 cpi 指向 pi@192.168.123.50，绑定源192.168.123.99。免密远程开发 PASS；插线后固定 IP 配置自动生效已配置，跨重启/反复插拔恢复 UNVERIFIED，不配置 Windows 自动登录终端或开机摄像头任务。
Pi 为 Debian/Linux 6.12.75+rpt-rpi-v8 aarch64、Python3.13.5；eth0 UP /24。Pi 时间返回2026-11-06，与Windows日期不一致，未修改；时长使用 monotonic。

## 摄像头和服务

Alcorlink USB 2.0 Camera：/dev/v4l/by-id/usb-Alcorlink_Corp._USB_2.0_Camera-video-index0 → /dev/video0，index1 为另一节点，不当作第二台相机。
v4l2-ctl 实际存在并确认640×480 MJPG支持，采集日志与Windows窗口确认实际640×480、MJPG。
原 cpi-vision.service enabled/active/running，PID632 占用video0；经用户明确允许临时停止后，确认 inactive/dead 且无占用，才启动预览。未修改unit、驱动、系统时钟或远端源码。
Windows Python3.10.9、OpenCV4.9.0、NumPy1.26.4；Pi原venv OpenCV4.13.0、NumPy2.4.4，未安装/升级依赖。
远端原physical_detect.py SHA256为81baaed142f0a93551444a67a80ffc02873723d8c89a0e22247c73ef3be104dd，本次未修改。

## 真实执行

| 测试 | 结果 | 实测证据 |
|---|---|---|
| A：GUI 90帧 | PASS | displayed=decoded=90，6.02s有效流，14.79FPS；decode_failures=0、preview_skipped=0；总耗时8.6s含启动清理 |
| B首轮及第一次重试 | FAIL | 摄像头select超时，读失败各1次，0帧；USB断开后以12M枚举；SSH诊断正常，不归因JPEG/网络 |
| B换口后持续预览 | PASS | 1579帧，75.00s有效流，21.04FPS；总77.0s，采集失败0、JPEG解码失败0、preview_skipped=0；本轮无中断 |
| Windows实际显示 | PASS | computer-use观察到真实视频、frame226→1004、颜色框/中心/相对坐标，非headless测试 |
| 窗口关闭/时长结束清理 | PASS | A/B stdout记录ssh_reaped=True、receiver_alive=False；每轮结束远端fuser无占用，无python -u -残留 |
| Q退出与服务恢复 | PASS | 独立复测838帧、40.56s、20.63FPS，早于45s上限正常退出；无异常，ssh_reaped=True、receiver_alive=False；远端fuser无占用且无残留worker，再恢复enabled/active/running，最终PID2087 |
| 真实红绿蓝物块 | PASS（定性） | Windows窗口实际观察到三色物块、对应颜色框/中心与dx、dy，用户确认颜色框正确；中心点有偏移，准确率与定位精度未验收，按用户要求留到通信完成后优化 |
| 精确端到端延迟 | UNVERIFIED | 单帧队列且本轮无跳帧；未做摄像头物理运动到显示的延迟测量，不能宣称零延迟 |
| 长期稳定/物理反复拔插恢复 | UNVERIFIED | 本次真实出现USB故障；换口恢复一次不代表彻底解决 |

USB故障：旧口usb1-1.2断开并以12M枚举、usb_set_interface failed(-32)；用户重新插接后新口usb1-1.4枚举为480M，之后75秒通过。vcgencmd get_throttled=0x0，不排除线材/接头/瞬态供电问题。未重写程序掩盖故障。
坐标沿用原函数dx=cx-290、dy=260-cy，无二次偏移；原算法和STM32固件均未修改。初始画面中的彩色LED也会被框出，后续真实物块画面独立核实；不在本阶段调整算法。

## 最小程序修改与验证

PI/preview_material.py：默认复用SSH配置/agent，保留可选--key，严格主机校验；新增--duration与有效流FPS、解码失败数、SSH/线程退出统计，输出实际FOURCC。沿用已有二进制JPEG协议、线程与单帧队列，不打开UART。
本地py_compile PASS，--help PASS，原协议/合成视觉unittest 6项PASS；这些测试不替代上表真实硬件验收。
完整日志仅在 .local/preview-*.log，未提交截图、测试视频、密钥或密码。
使用步骤见windows_preview.md。原先已有四个firmware工程/生成配置文件本地修改，保留且不纳入本次提交。

交互物块预览期间905帧后再次出现USB断开并报Camera frame read failed（该轮FAIL，非Q正常退出证据）；设备重枚举后原服务恢复采集。随后独立短时Q复测成功并确认资源释放。三色物块示例日志：Red=(-121,4)、Green=(-49,13)、Blue=(41,18)，后续位置变化时偏差也变化，不代表像素中心精度已标定。
Q测试发现中文输入法会拦截字母为拼音组合；切换英文输入模式后正常提前退出。使用者可先切英文，再按Q；Esc也可退出。
最终服务恢复为enabled/active/running，且单独检查摄像头仅归服务PID2087持有；不会把恢复后服务正常占用误判为预览泄漏。
