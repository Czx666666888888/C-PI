# 嵌入式固件审阅（静态）

来源：上传的 DJI C board.zip。未经实机编译、烧录验证。

- MCU：STM32F407；工具：STM32CubeMX + Keil MDK-ARM。
- Core/Src/freertos.c：创建底盘等 FreeRTOS 任务。
- StartTask02：CAN、BMI088、底盘更新逻辑。
- StartTask04：CRSF 遥控，周期调用 chassis_set_target(vx,vy,wz)。
- Core/Src/usart.c：USART1、3、6 初始化；USART1 已用于 CRSF 接收。
- Lib/chassis 与 Lib/chassis_can：底盘控制与 CAN 电机驱动。
- Lib/bmi088、Lib/Lift、Lib/SG90：传感器与机构相关模块。

## 接入树莓派前需处理
1. 以 USART3 为候选通信口，先核对实物引脚和其他功能占用。
2. 不能将 UART 初始化等同于已有上位机消息解析。
3. 需要遥控/自主模式仲裁，避免互相覆盖速度目标。
4. 明确速度单位、坐标系、限幅和反馈状态。
5. 通信失联后自主控制目标应归零。

完整固件需由本地工程推送，才能在 GitHub 被 Codex 直接读取。
