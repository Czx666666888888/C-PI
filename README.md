# C-PI — 智能搬运机器人

2027 工程实践与创新能力大赛智能搬运赛项项目。

## 控制架构
- DJI C 板 STM32F407 + FreeRTOS：CAN 电机、底盘运动、IMU、执行机构。
- 树莓派上位机（开发中）：二维码、相机识别、视觉位置矫正、全场定位接入、任务规划。
- 串口（待开发）：上位机发运动目标，下位机返回运行状态。

## 仓库导入说明
STM32 原始工程已导入 `firmware/`，尚未在本环境编译或实机验证。树莓派原有 Python 源码已归档到 `raspberry_pi/legacy/`，迁移审阅见 `raspberry_pi/README.md`，不代表新通信链路已实现。

## 文档
- AGENTS.md：Codex 项目约束
- docs/code_audit.md：已有固件审阅
- docs/communication_design.md：串口通信草案
- docs/rules_summary.md：赛题摘要
- docs/roadmap.md：实施路线
- docs/pi_network.md：树莓派固定有线地址与 SSH 验证记录
- raspberry_pi/README.md：原始视觉/串口源码迁移、审阅与待验证项

## 树莓派 V1
视觉与 UART 联合服务见 [运行及部署](raspberry_pi/README.md#v1-联合服务) 和 [V1 协议](docs/uart_v1.md)。
本阶段仅发送视觉观测，不修改固件、不控制运动。实物 C 板联调与固件超时停车 UNVERIFIED。
