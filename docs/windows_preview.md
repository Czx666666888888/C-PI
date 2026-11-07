# Windows 与树莓派实时预览

使用 Windows OpenSSH 配置中的 cpi 别名或 192.168.123.50，专用公钥已安装。私钥只保存在 Windows 用户 .ssh 下，不提交仓库。
本机物理以太网设 192.168.123.99/24，无网关/DNS；Pi eth0 为 192.168.123.50/24。静态配置持久保存，插线后恢复直连网络；无需保持交互式 SSH 窗口。是否能跨重启、反复插拔自动恢复仍须单独实测。
回到其他有线网络时，将该以太网 IPv4 地址和 DNS 改回自动获取；不更改 WLAN。

## 登录和检查

```powershell
ssh -o BatchMode=yes cpi hostname
ssh cpi 'systemctl status cpi-vision --no-pager'
ssh cpi 'ls -l /dev/v4l/by-id/; fuser -v /dev/video0'
```

仅在用户允许临时停止服务后，执行：

```powershell
ssh cpi 'sudo -n systemctl stop cpi-vision'
ssh cpi 'fuser -v /dev/video0'
python .\PI\preview_material.py --host cpi --frames 90
python .\PI\preview_material.py --host cpi --duration 65
# 交互预览，切英文输入法后按 Q，或按 Esc / 关闭窗口退出
python .\PI\preview_material.py --host cpi
```

测试结束检查并恢复原先 active 的服务（服务本身原有 combined 模式会重新打开 UART；预览程序不打开 UART）：

```powershell
ssh cpi 'fuser -v /dev/video0; ps -eo pid,args | grep "[p]ython -u -"'
ssh cpi 'sudo -n systemctl start cpi-vision; systemctl show cpi-vision -p ActiveState -p SubState -p MainPID'
```

原 enabled 状态保持不变，不改 systemd unit。若原先 inactive，不应自行启动。
程序复用 /home/pi/opencv_test/.venv/bin/python 与 physical_detect.py，远端源码不覆盖；640×480、MJPG、JPEG quality80，经 SSH 二进制长度帧传送。
Windows 后台线程解码，单帧队列保留最新画面；preview_skipped 代表主动替换等待显示帧，不等于摄像头丢帧。duration 从首个显示帧开始统计；STOP 汇总 FPS 为显示间隔平均值，不是物理摄像头帧率。
默认复用 OpenSSH config/agent，可选 --key 显式指定密钥，严格主机校验。不会打开串口、发送运动命令或修改固件。
颜色框、中心和偏差来自原检测函数：dx=cx-290、dy=260-cy，不再次减参考点。算法也可能识别背景灯光，需用真实物块单独验收。

## USB 排查

```powershell
ssh cpi 'lsusb -t; journalctl -k -n 30 --no-pager; vcgencmd get_throttled'
```

本次曾出现 USB disconnect、12M 低速重新枚举、select timeout，造成读取失败；换口重新插接后恢复 480M。不要将该故障归因于 JPEG 解码或 SSH，也不能据一次恢复宣称长期稳定。
不安装驱动、不改内核、不修改系统时间；Pi 时钟与 Windows 不一致，时长以 monotonic 计。
日志保存在根目录 .local（Git 忽略），不提交视频、截图或密钥。实际结果另见 windows_preview_validation.md。
