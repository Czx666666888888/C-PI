# C-PI 智能搬运机器人

2027 智能搬运项目：Windows 开发与 Git，树莓派运行视觉与 UART，DJI C 板 STM32F407 执行实时控制。

```text
C-PI/（本机文件夹名 26C-PI）
├── AGENTS.md / README.md / .gitignore
├── firmware/   Core、Drivers、Middlewares、Lib、MDK-ARM、DJI C board.ioc
├── PI/         legacy、communication、vision、config、tests、scripts、systemd
├── tools/      build_firmware.ps1、check_workspace.ps1、deploy_pi.ps1、pull_pi.ps1
└── docs/
```

本地 DJI C board(1)/ 原件与 .migration-remote/ 下载暂存保留、已忽略。开发使用 firmware/ 和 PI/。
双击 `firmware/MDK-ARM/DJI C board.uvprojx` 打开 Keil；根目录 PowerShell：

```powershell
./tools/check_workspace.ps1
./tools/build_firmware.ps1
python -m unittest discover -s PI/tests -v
./tools/deploy_pi.ps1
./tools/deploy_pi.ps1 -Apply
./tools/pull_pi.ps1
./tools/pull_pi.ps1 -Apply
```

工具不硬编码盘符；Keil 从注册表/PATH/KEIL_UV4 发现，也可传 -KeilPath。同步依赖 Python 3、Windows OpenSSH 和 Pi python3，支持 -Target user@host -RemoteDir /absolute/path。
默认预检查；-Apply 才传输。不删除文件，不重启服务。首次遇到不同内容拒绝覆盖；之后仅替换匹配 .local/ 上次共同哈希的文件；回拉有 PI 未提交修改时阻止写入。丢失状态文件后恢复首次同步保护。
首次 SSH 需人工验证主机指纹并配置密钥，脚本严格检查 known_hosts 并使用 BatchMode。排除 venv、缓存、日志、常见密钥与 .env，凭据不得放入源码。
避免两端同时编辑。单文件替换原子，整批非事务；失败请复查后重跑。旧 PI/scripts/deploy.sh 保留作历史工具，缺少上述保护，Windows 使用 tools 入口。

2026-11-07 实际 Keil rebuild **PASS，0 Errors / 14 Warnings**。ST-Link 烧录 **UNVERIFIED**，本次未烧录。Pi 免密 SSH 与 Windows 实时预览已验证；同步部署脚本端到端实机仍 **UNVERIFIED**，USB 摄像头存在间歇断连，详见本次预览验收。
详见 [迁移验收](docs/windows_migration.md)、[固件说明](firmware/README.md)、[Pi 说明](PI/README.md)、[V1 协议](docs/uart_v1.md)。
已有 Pi 实现视觉偏差、UART 115200 8N1、HEARTBEAT/TARGET、接收 TARGET_ACK/STATUS、CRC16、重连、valid=0 和 systemd unit。Windows 6 个协议/合成视觉测试通过；C 板V1协议与手/自动红色夹爪触发已实现并编译，实板收发与动作待烧录后验证。
旧二维码、相机预览和串口程序保留在 legacy，遗留脚本有旧环境和未定义依赖，不代表全部可运行。历史实机证据见 docs/pi_uart_v1_validation.md，当前硬件须重验。
本次 feature/windows-workspace 基于 feature/pi-uart-v1 的 e122438，含 main 和视觉分支历史；每次任务测试后 Commit/Push 并核对 SHA，不 force push，不自动合并 main。约束见 AGENTS.md。

Windows 免密 SSH 与摄像头预览入口见 [使用说明](docs/windows_preview.md) 和 [本次实测](docs/windows_preview_validation.md)。

STM32 手/自动模式、V1 收发与红色夹爪触发见 [开发验收](docs/stm32_uart_v1_validation.md)。
