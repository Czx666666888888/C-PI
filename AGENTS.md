# AGENTS.md — C-PI

## 项目
DJI C 板 STM32F407 是实时运动控制下位机；树莓派是负责视觉识别、定位矫正与任务规划的上位机。2027 智能搬运竞赛。

## 开发规则
1. 阅读 README、docs/code_audit.md 和 docs/rules_summary.md 后才开始修改代码。
2. 保留已可运行的底盘逻辑。未经授权不改电机参数、外设引脚、CubeMX 配置，不做大规模重构。
3. USART1 已用于 CRSF 遥控，不可擅自作为上位机通信口；USART3 是候选，但需要核对实际接口。
4. 当前遥控任务已调用 chassis_set_target(vx,vy,wz)；自主模式必须统一仲裁控制权，不能两个线程抢写。
5. 通信应设计清楚长度、序号、单位、坐标系、CRC、超时。断线要让自主运动停止。
6. 树莓派负责视觉与任务调度；C 板负责实时电机闭环。
7. 只做当前任务必要的测试，避免无关审查；但通信帧检验和超时停车不可省。
8. 实机未验证时必须标记 UNVERIFIED。不得声称没有运行的测试通过。
9. 提交说明必须包含修改文件、运行的验证命令、结果和遗留问题。

## 协作
用户确定目标；ChatGPT 分析设计及验收；Codex 在功能分支实施并 Push；ChatGPT 复核代码和结果。

## Windows 长期工作流
- Windows 开发和 Git；Pi 运行视觉、UART、systemd。代码统一 PI/，远端 /home/pi/cpi-vision，保留 /home/pi/opencv_test。
- 每次任务结束检查 diff、运行相关测试、仅 add 任务文件、commit、push 对应功能分支，并 ls-remote 核对 SHA。用户已长期授权 Push，无需再次确认；失败必须报告。禁止 force push，不直接合并未经测试的 main。
- main 为稳定基线；feature/pi-source-migration 为历史视觉迁移；feature/pi-uart-v1 为 Pi V1；feature/stm32-uart-v1 为后续固件通信；本次 feature/windows-workspace 基于最新 Pi 成果。
- 不覆盖未提交修改；禁止 reset --hard、git clean -fd。目录整理须核查工程相对路径。DJI C board(1)/ 是保留原件，不作开发入口。
- Keil 入口 firmware/MDK-ARM/DJI C board.uvprojx；使用 tools/build_firmware.ps1。不擅自更换 HAL、FreeRTOS、Pack、编译器或迁移 GCC；禁止伪造编译成功。
- 不提交 .o/.axf/.hex、日志、缓存或密钥。本地输出保留；烧录会驱动电机的固件须另获用户确认。
- tools/deploy_pi.ps1 和 pull_pi.ps1 默认预检查，-Apply 才写入；哈希冲突人工合并，不覆盖独立修改，不自动重启服务。
- V1 以 docs/uart_v1.md 和 PI/communication/protocol.py 为准，本阶段不实现旧草案 HELLO/SET_VELOCITY。
- 夹爪参考点 (290,260)，dx=cx-290、dy=260-cy；get_color_blocks_positions 已返回偏差，禁止再次计算。不擅改视觉算法。当前授权自动颜色动作仅限：红色夹爪闭合、绿色张开、蓝色固定0.25m/s前进最多1秒；目标失效/变色停车，手动优先，不用dx/dy导航。
- USART1 保留 CRSF；USART3 回调不能破坏 USART1；StartTask04 已写底盘目标，后续须统一仲裁和失联停车。
