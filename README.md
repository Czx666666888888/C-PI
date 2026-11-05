# C-PI — 智能搬运机器人

2027 工程实践与创新能力大赛智能搬运赛项项目。

## 控制架构
- DJI C 板 STM32F407 + FreeRTOS：CAN 电机、底盘运动、IMU、执行机构。
- 树莓派上位机（开发中）：二维码、相机识别、视觉位置矫正、全场定位接入、任务规划。
- 串口（待开发）：上位机发运动目标，下位机返回运行状态。

## 仓库导入说明
当前先发布项目文档。完整 STM32 工程仍需从准备好的本地 Git 工程推送到 GitHub；在实际推送前不要认为固件源码已上传。

## 文档
- AGENTS.md：Codex 项目约束
- docs/code_audit.md：已有固件审阅
- docs/communication_design.md：串口通信草案
- docs/rules_summary.md：赛题摘要
- docs/roadmap.md：实施路线

## 下一步
优先建立树莓派与 C 板双向心跳、速度目标、状态反馈和超时停车。
