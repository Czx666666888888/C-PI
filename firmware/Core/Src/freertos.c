/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "chassis.h"
#include "chassis_can.h"
#include "X_V2.h"
#include "can.h"
#include "bsp_can.h"
#include "6020_pid.h"
#include "OLED.h"
#include "bsp_adc.h"
#include "ist8310driver.h"
#include "BMI088driver.h"
#include "crsf.h"
#include "SG90.h"
#include "pi_link.h"
#include "usart.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define CHASSIS_LOOP_DT_S      0.002f
#define CHASSIS_FORWARD_SPEED  0.25f

/* ── 遥控(CRSF)通道映射: 按遥控器通道顺序调整 (0 起) ──
 * 实测通道值:
 *   CH0 左右平移    居中=0992
 *   CH1 前进/后退   居中=0992
 *   CH2 油门/转速   停=0174, 满=1811 (非回中)
 *   CH3 旋转        居中=0992
 *   CH4 夹爪舵机    三段拨杆(191/997/1792)
 *   CH5 电机6       三段拨杆(191/997/1792) */
#define REMOTE_CH_STRAFE    0    /* 左右平移方向 (vy) */
#define REMOTE_CH_FORWARD   1    /* 前进/后退方向 (vx) */
#define REMOTE_CH_THROTTLE  2    /* 油门: 全局速度 174=停 1811=满速 */
#define REMOTE_CH_ROTATE    3    /* 旋转 (wz) */
#define REMOTE_CH_ARM       4    /* 通道5: 夹爪舵机三段拨杆 (原解锁开关, 已废弃) */

#define REMOTE_SIGN_VX      1.0f  /* 前进方向反了改 -1.0f */
#define REMOTE_SIGN_VY      1.0f
#define REMOTE_SIGN_WZ      -1.0f /* 自转方向: 摇杆左→车体左转(逆时针) */

#define REMOTE_MAX_VX       1.2f  /* 与底盘上限 g_max_vx 一致 */
#define REMOTE_MAX_VY       1.2f
#define REMOTE_MAX_WZ       4.0f
#define REMOTE_DEADZONE     0.05f
#define REMOTE_LINK_TIMEOUT_MS  500   /* 掉线判据 */

/* 油门(非回中)零点和量程 */
#define REMOTE_THROTTLE_MIN     174
#define REMOTE_THROTTLE_RANGE   1637   /* = CRSF_CH_MAX(1811) - 174 */
#define REMOTE_THROTTLE_DEADZONE  40   /* 油门死区: <174+40 视为完全停止(防蠕动) */

/* ── 塔吊(塔吊)遥控: CH7 开关切换模式, 油门→升降(电机#5), CH3→旋转(6020) ── */
#define REMOTE_CH_MODE        6    /* CH7 开关: 拨动后切换塔吊模式 */
#define REMOTE_MODE_THRESHOLD 1500 /* >1500 视为塔吊模式 */

#define TOWER_LIFT_MOTOR_ADDR  5U        /* 升降电机(张大头 X_V2 #5)地址 */
#define TOWER_LIFT_MAX_RPM     750.0f    /* 升降最大转速(RPM): 原 500 提高到 1.5 倍 */
#define TOWER_LIFT_ACC_RPMPS   500U      /* 升降加减速(RPM/s) */
#define TOWER_LIFT_DIR_UP      0U        /* 前进(升)方向: 0=CW, 1=CCW。方向反了把 UP/DOWN 两值互换 */
#define TOWER_LIFT_DIR_DOWN    1U        /* 后退(降)方向 */
#define TOWER_ROTATE_MAX_RPM   120.0f    /* 6020 旋转最大转速 */
#define TOWER_ROTATE_SIGN      -1.0f     /* 旋转方向反了改 +1.0f */
#define TOWER_HOLD_ANGLE_KP    0.8f      /* 锁位外环: 位置误差→转速指令 (RPM/count) */
#define TOWER_HOLD_DAMP        0.5f      /* 锁位外环速度阻尼 (转速指令 RPM / 实测 RPM) */
#define TOWER_HOLD_MAX_SPEED   150.0f    /* 锁位最大纠偏转速 (RPM) */
#define TOWER_HOLD_ERR_MAX     2048.0f   /* 锁位位置误差钳位(±90°) */

/* ── 夹爪舵机(SG90, PE9/TIM1_CH1): CH5(通道5=idx4) 三段拨杆 → 三档角度 ──
 * 实测拨杆三值: 0191 / 0997 / 1792。阈值取相邻两档中点。 */
#define TOWER_GRIP_CH_INDEX    REMOTE_CH_ARM  /* 通道5 = idx4 (复用 REMOTE_CH_ARM) */
#define TOWER_GRIP_LOW_TH      600    /* <600  判为下段 191 */
#define TOWER_GRIP_HIGH_TH     1300   /* >1300 判为上段 1792 */
#define TOWER_GRIP_ANGLE_LOW   (-80)  /* 下段 191 → 夹爪闭合(抓取) */
#define TOWER_GRIP_ANGLE_MID   (-40)  /* 中位 997 → 夹爪半开 */
#define TOWER_GRIP_ANGLE_HIGH  (0)    /* 上段 1792 → 夹爪张开 */

/* ── 电机6(张大头 X_V2 电机 #6): 通道6=idx5 三段拨杆 → 三态控制 ──
 * 遥控器把三段拨杆映射到通道6, 实测值同通道5: 0191 / 0997 / 1792。
 *   0191 → 正方向缓速;  0997 → 停止并自锁;  1792 → 反方向缓速。
 * 该通道代码里原为空闲, 与夹爪舵机(通道5)互不影响。 */
#define REMOTE_CH_MOTOR6      5U   /* 通道6 = idx5 (空闲通道) */
#define MOTOR6_ADDR           6U   /* CAN 总线电机地址 */
#define MOTOR6_SLOW_RPM       20.0f  /* 缓速转速(RPM): 啮合阻力不均匀, 已降三次(200→60→30→20),
                                          速度越低扭矩余量越大, 齿条遇紧点越不易被拖慢 */
#define MOTOR6_ACC_RPMPS      100U   /* 加减速(RPM/s): 缓加速, 起步更平稳 */
#define MOTOR6_MAX_CURRENT_MA 6000U  /* 闭环模式最大电流(mA), 与底盘四轮一致(可调) */
#define MOTOR6_DIR_FWD        0U   /* 正方向: 0=CW, 1=CCW。方向反了把 FWD/REV 两值互换 */
#define MOTOR6_DIR_REV        1U   /* 反方向 */
#define MOTOR6_SW_LOW_TH      600U  /* <600  判为下段 191 → 正方向 */
#define MOTOR6_SW_HIGH_TH     1300U /* >1300 判为上段 1792 → 反方向 */

/* 通道原始值(中位 992)归一化到 [-1,1], 带死区 */
#define REMOTE_NORM(ch, out)                                         \
    do {                                                             \
        int32_t _d = (int32_t)(ch) - CRSF_CH_CENTER;                 \
        float   _v = (float)_d / 820.0f;                             \
        if (_v >  1.0f) { _v =  1.0f; }                              \
        if (_v < -1.0f) { _v = -1.0f; }                              \
        if (_v > -REMOTE_DEADZONE && _v < REMOTE_DEADZONE) { _v = 0.0f; } \
        (out) = _v;                                                  \
    } while (0)

/* 油门(非回中)归一化: 174+死区以下→0.0, 1811→1.0 */
#define REMOTE_THROTTLE_NORM(ch, out)                                 \
    do {                                                              \
        int32_t _t = (int32_t)(ch) - REMOTE_THROTTLE_MIN - REMOTE_THROTTLE_DEADZONE; \
        float   _v = (float)_t / (float)(REMOTE_THROTTLE_RANGE - REMOTE_THROTTLE_DEADZONE); \
        if (_v < 0.0f) { _v = 0.0f; }                                 \
        if (_v > 1.0f) { _v = 1.0f; }                                 \
        (out) = _v;                                                   \
    } while (0)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
static volatile uint8_t g_chassis_ready = 0U; /* 完成原底盘初始化后才允许蓝色前进。 */
volatile control_mode_t g_control_mode = CONTROL_MODE_AUTO;
volatile uint8_t g_auto_red_grab_done = 0U;
volatile uint8_t g_tower_mode_active = 0U; /* 塔吊模式激活: 1=底盘任务停发CAN, 交 TOWER_Task 独占总线 */
extern pid_struct_t gimbal_yaw_speed_pid;  /* 6020 速度环 PID (塔吊旋转用) */
volatile int16_t g_6020_voltage_cmd = 0;   /* 最近一拍下发给 6020 的电压指令(诊断: OLED 塔吊模式第3行) */
/* USER CODE END Variables */
osThreadId defaultTaskHandle;
osThreadId Chassis_TaskHandle;
osThreadId Output_TaskHandle;
osThreadId UART_RX_TaskHandle;
osThreadId MAIN_TaskHandle;
osThreadId TOWER_TaskHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
/* GM6020 辅助函数 — 暂时禁用，恢复 TOWER_Task 时解开 */
#if 0
static void gimbal_hold_control(uint32_t ms);
static void gimbal_wait_reached(uint32_t timeout_ms);
#endif
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void const * argument);
void StartTask02(void const * argument);
void StartTask03(void const * argument);
void StartTask04(void const * argument);
void StartTask05(void const * argument);
void StartTask06(void const * argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
  /* place for user code */
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 256);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* definition and creation of Chassis_Task */
  osThreadDef(Chassis_Task, StartTask02, osPriorityHigh, 0, 512);
  Chassis_TaskHandle = osThreadCreate(osThread(Chassis_Task), NULL);

  /* definition and creation of Output_Task */
  osThreadDef(Output_Task, StartTask03, osPriorityLow, 0, 128);
  Output_TaskHandle = osThreadCreate(osThread(Output_Task), NULL);

  /* definition and creation of UART_RX_Task */
  osThreadDef(UART_RX_Task, StartTask04, osPriorityAboveNormal, 0, 256);
  UART_RX_TaskHandle = osThreadCreate(osThread(UART_RX_Task), NULL);

  /* definition and creation of MAIN_Task */
  osThreadDef(MAIN_Task, StartTask05, osPriorityNormal, 0, 256);
  MAIN_TaskHandle = osThreadCreate(osThread(MAIN_Task), NULL);

  /* definition and creation of TOWER_Task */
  osThreadDef(TOWER_Task, StartTask06, osPriorityNormal, 0, 256);
  TOWER_TaskHandle = osThreadCreate(osThread(TOWER_Task), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* --- OLED 底盘轮速诊断 ---
   * Line1: RC=链路(Y/N) B=累计字节 F=有效帧
   * Line2~3: 目标轮速 T0~T3(代码期望下发的底盘坐标系转速, 带符号)
   * Line4~5: 实测轮速 M0~M3(编码器回传)
   * Line6: 轮速离散度 = max|w| - min|w| (RPM)
   * 判断: 目标≠0 而实测=0 → 电机没转(接线/使能/地址); 目标=0 → 遥控没给指令
   * Line8: CAN 接收诊断 */

  float    wrpm[4];
  float    wcmd[4];
  char     line[22];
  uint8_t  i;

  OLED_Clear();

  for(;;)
  {
    chassis_get_wheel_rpm(wrpm);
    chassis_get_wheel_cmd_rpm(wcmd);

    /* Line 1: 诊断状态 */
    snprintf(line, sizeof(line), "RC:%c B:%lu F:%lu",
             crsf_is_linked(REMOTE_LINK_TIMEOUT_MS) ? 'Y' : 'N',
             (unsigned long)crsf_byte_count(),
             (unsigned long)crsf_frame_count());
    OLED_ShowString_6x8(1, 1, line);
    for (i = 0; line[i] != '\0'; i++);
    for (; i < 21U; i++) { OLED_ShowChar_6x8(1, (uint8_t)(1U + i), ' '); }

    /* Line 2~3: 目标轮速(前左 前右 后左 后右)。
     * 塔吊模式下底盘停转, 借用这两行显示 6020 诊断:
     *   C=0x208 回包计数(为0→6020不在总线上/没供电/线松),
     *   sp=实测转速, V=本拍电压指令, d=0x1FF TX超时丢弃计数(>0→命令没发出去)。 */
    if (g_tower_mode_active)
    {
      snprintf(line, sizeof(line), "G C:%04u sp:%+4d", (unsigned)can_cnt, (int)motor_yaw_info.rotor_speed);
      OLED_ShowString_6x8(2, 1, line);
      for (i = 0; line[i] != '\0'; i++);
      for (; i < 21U; i++) { OLED_ShowChar_6x8(2, (uint8_t)(1U + i), ' '); }

      snprintf(line, sizeof(line), "G V:%+5d d:%04u", (int)g_6020_voltage_cmd, (unsigned)g_6020_tx_drop_count);
      OLED_ShowString_6x8(3, 1, line);
      for (i = 0; line[i] != '\0'; i++);
      for (; i < 21U; i++) { OLED_ShowChar_6x8(3, (uint8_t)(1U + i), ' '); }
    }
    else
    {
      snprintf(line, sizeof(line), "T0:%+4d T1:%+4d", (int)wcmd[0], (int)wcmd[1]);
      OLED_ShowString_6x8(2, 1, line);
      for (i = 0; line[i] != '\0'; i++);
      for (; i < 21U; i++) { OLED_ShowChar_6x8(2, (uint8_t)(1U + i), ' '); }

      snprintf(line, sizeof(line), "T2:%+4d T3:%+4d", (int)wcmd[2], (int)wcmd[3]);
      OLED_ShowString_6x8(3, 1, line);
      for (i = 0; line[i] != '\0'; i++);
      for (; i < 21U; i++) { OLED_ShowChar_6x8(3, (uint8_t)(1U + i), ' '); }
    }

    /* Line 4~5: 实测轮速(编码器回传) */
    snprintf(line, sizeof(line), "M0:%+4d M1:%+4d", (int)wrpm[0], (int)wrpm[1]);
    OLED_ShowString_6x8(4, 1, line);
    for (i = 0; line[i] != '\0'; i++);
    for (; i < 21U; i++) { OLED_ShowChar_6x8(4, (uint8_t)(1U + i), ' '); }

    snprintf(line, sizeof(line), "M2:%+4d M3:%+4d", (int)wrpm[2], (int)wrpm[3]);
    OLED_ShowString_6x8(5, 1, line);
    for (i = 0; line[i] != '\0'; i++);
    for (; i < 21U; i++) { OLED_ShowChar_6x8(5, (uint8_t)(1U + i), ' '); }

    /* Line 6: 轮速离散度(max|w| - min|w|), 直观衡量四轮一致性 */
    {
      float maxv = wrpm[0] < 0.0f ? -wrpm[0] : wrpm[0];
      float minv = maxv;
      for (i = 1U; i < 4U; i++)
      {
        float a = wrpm[i] < 0.0f ? -wrpm[i] : wrpm[i];
        if (a > maxv) maxv = a;
        if (a < minv) minv = a;
      }
      snprintf(line, sizeof(line), "spd:%+4d", (int)(maxv - minv));
      OLED_ShowString_6x8(6, 1, line);
      for (i = 0; line[i] != '\0'; i++);
      for (; i < 21U; i++) { OLED_ShowChar_6x8(6, (uint8_t)(1U + i), ' '); }
    }

    /* Line 7: 电机回包地址(诊断后轮)。
     * can_id=0 → 该电机从未回包(没响应/地址错/没上电);
     * can_id=3/4 且实测=0 → 电机已响应但卡死/负载太重/扭矩不够。 */
    snprintf(line, sizeof(line), "ID:%u %u %u %u",
             (unsigned)g_chassis_motors[0].can_id,
             (unsigned)g_chassis_motors[1].can_id,
             (unsigned)g_chassis_motors[2].can_id,
             (unsigned)g_chassis_motors[3].can_id);
    OLED_ShowString_6x8(7, 1, line);
    for (i = 0; line[i] != '\0'; i++);
    for (; i < 21U; i++) { OLED_ShowChar_6x8(7, (uint8_t)(1U + i), ' '); }

    /* Line 8: 电机6诊断 — 回包计数 + 实测转速(带符号, CCW=正/CW=负) + 拨杆通道值。
     * 例 "6:123 +60 m6:1792" = 地址6收到123帧回包, 实测转速+60RPM, 拨杆在上段。 */
    snprintf(line, sizeof(line), "6:%u %+d m6:%04u",
             (unsigned)g_can_rx1_ext_addr_count[6],
             (int)g_motor6_speed_rpm,
             (unsigned)crsf_get_channel(REMOTE_CH_MOTOR6));
    OLED_ShowString_6x8(8, 1, line);
    for (i = 0; line[i] != '\0'; i++);
    for (; i < 21U; i++) { OLED_ShowChar_6x8(8, (uint8_t)(1U + i), ' '); }

    osDelay(50);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartTask02 */
/**
* @brief Function implementing the Chassis_Task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask02 */
void StartTask02(void const * argument)
{
  /* USER CODE BEGIN StartTask02 */
  /* --- 底盘控制任务: CAN + 陀螺仪读取 + 底盘速度环, 速度目标由遥控任务下发 --- */

  /* CAN 总线启动（在调度器上下文中执行，避免中断在 FreeRTOS 启动前触发）*/
  HAL_CAN_Start(&hcan1);
  HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
  HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO1_MSG_PENDING);

  uint32_t heartbeat_cnt = 0U;
  uint32_t diag_cnt = 0U;
  extern uint32_t g_chassis_can_tx_drop_count;

  /* PH10=LED3 亮一下表示 Chassis_Task 已启动 */
  HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_SET);
  osDelay(50);
  HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET);

  chassis_init();

  /* 陀螺仪零偏标定(车辆静止时) + 使能陀螺仪偏航闭环 */
  chassis_gyro_calibrate();
  chassis_set_gyro_yaw_enable(true);

  /* 陀螺仪修正增益(实地可调):
   *   航向保持   —— 平移时锁定航向, 修正"左右平移走斜"；
   *   自转中心保持 —— 原地旋转时锁定空间位置, 修正"打着转飘移"。 */
  chassis_set_heading_hold_gain(3.0f);
  chassis_set_rotation_hold_gain(1.5f);

  /* PH10=LED3 再亮一下表示初始化完成 */
  HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_SET);
  osDelay(50);
  HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET);

  /* 用 DWT 周期计数器实测控制周期(实际≈2.3ms, 而非假设的2ms),
     否则陀螺航向积分会系统性偏低, 航向保持/自转中心保持都会失准 */
  uint32_t last_cyc = DWT->CYCCNT;
  g_chassis_ready = 1U;

  for (;;)
  {
    uint32_t now_cyc = DWT->CYCCNT;
    float loop_dt_s = (float)(now_cyc - last_cyc) / (float)SystemCoreClock;
    last_cyc = now_cyc;
    if (loop_dt_s < 0.001f || loop_dt_s > 0.01f)
    {
      loop_dt_s = CHASSIS_LOOP_DT_S;   /* 首拍/异常时兜底, 防止 0 或超长 dt */
    }

    /* ── CAN Bus-Off 检测与恢复 ───────────────── */
    uint32_t can_esr = hcan1.Instance->ESR;
    if (can_esr & CAN_ESR_BOFF)
    {
      HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
      HAL_CAN_Stop(&hcan1);
      osDelay(10);
      __HAL_CAN_CLEAR_FLAG(&hcan1, CAN_FLAG_BOF);
      HAL_CAN_Start(&hcan1);
      HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
      HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO1_MSG_PENDING);
      g_chassis_can_tx_drop_count = 0U;
      HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
      osDelay(20);
      continue;
    }
    else if (can_esr & CAN_ESR_EPVF)
    {
      static uint8_t ep_toggle = 0U;
      ep_toggle ^= 1U;
      if (ep_toggle)
        HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
      else
        HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
    }
    else if (can_esr & CAN_ESR_EWGF)
    {
      static uint8_t ew_cnt = 0U;
      ew_cnt++;
      if (ew_cnt >= 100U) { HAL_GPIO_TogglePin(LED2_GPIO_Port, LED2_Pin); ew_cnt = 0U; }
    }
    /* ── CAN 状态检测结束 ─────────────────────── */

    /* 单读源: 每循环读陀螺仪(供偏航闭环控制用) */
    BMI088_gyro_read(g_imu_gyro);

    chassis_update(loop_dt_s);

    /* ── 诊断 LED ─────────────────────────────── */
    heartbeat_cnt++;
    if (heartbeat_cnt >= 250U)
    {
      HAL_GPIO_TogglePin(LED3_GPIO_Port, LED3_Pin);
      heartbeat_cnt = 0U;
    }

    diag_cnt++;
    if (diag_cnt >= 500U)
    {
      uint8_t fb_ok = 0U;
      for (uint8_t m = 0U; m < 4U; m++)
      {
        if (g_chassis_motors[m].can_id != 0U) { fb_ok = 1U; break; }
      }
      if (fb_ok)
        HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
      diag_cnt = 0U;
    }
    /* ── 诊断 LED 结束 ─────────────────────────── */

    osDelay(2);
  }
  /* USER CODE END StartTask02 */
}

/* USER CODE BEGIN Header_StartTask03 */
/**
* @brief Function implementing the Output_Task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask03 */
void StartTask03(void const * argument)
{
  /* USER CODE BEGIN StartTask03 */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1000);
  }
  /* USER CODE END StartTask03 */
}

/* USER CODE BEGIN Header_StartTask04 */
/**
* @brief Function implementing the UART_RX_Task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask04 */
void StartTask04(void const * argument)
{
  /* USER CODE BEGIN StartTask04 */
  /* --- 遥控接收(CRSF)任务: 解析 ELRS 接收机数据 → 底盘速度指令 ---
   * 数据流: 遥控器 → ELRS 接收机(CRSF) → USART1(PA9=TX, PB7=RX) → crsf.c 解析
   * 本任务周期读取通道值并映射为 chassis_set_target(vx, vy, wz)。
   * 掉线切换自动时归零一次，此后不抢占未来自动程序的底盘目标。 */
  uint16_t ch_thr, ch_fwd, ch_str, ch_rot, ch_mode;
  float thr, nvx, nvy, nwz;
  pi_target_t auto_target;
  uint8_t blue_armed = 1U, blue_running = 0U;
  uint32_t blue_start_tick = 0U;
  static uint8_t motor6_initialized = 0U;  /* 电机6首次使能标志(仅塔吊模式可控, 与底盘驱动分开) */
  for(;;)
  {
    if (!crsf_is_linked(REMOTE_LINK_TIMEOUT_MS))
    {
      /* 手动→自动仅停车一次，自动分支不持续覆盖底盘目标。 */
      if (g_control_mode == CONTROL_MODE_MANUAL)
      {
        chassis_set_target(0.0f, 0.0f, 0.0f);
      }
      /* 电机6仍保留原有掉线停机重发，只有底盘目标改为切换时归零。 */
      if (motor6_initialized) { X_V2_Stop_Now(MOTOR6_ADDR, false); }
      g_control_mode = CONTROL_MODE_AUTO;
      pi_link_get_target(&auto_target);
      if (!auto_target.valid || auto_target.color != 3U)
      {
        if (blue_running) chassis_set_target(0.0f, 0.0f, 0.0f);
        blue_running = 0U;
        blue_armed = 1U;
      }
      else if (g_chassis_ready)
      {
        if (blue_armed)
        {
          blue_start_tick = HAL_GetTick();
          blue_running = 1U;
          blue_armed = 0U;
        }
        if (blue_running)
        {
          if ((uint32_t)(HAL_GetTick() - blue_start_tick) >= 1000U)
          {
            chassis_set_target(0.0f, 0.0f, 0.0f);
            blue_running = 0U;
          }
          else
          {
            /* 非阻塞计时；目标失效/变色提前停，同色持续发送不会续时。 */
            chassis_set_target(CHASSIS_FORWARD_SPEED, 0.0f, 0.0f);
          }
        }
      }
      osDelay(10);
      continue;
    }

    blue_running = 0U;
    blue_armed = 1U; /* 取消自动计时，后续底盘目标完全由原手动分支接管。 */
    g_control_mode = CONTROL_MODE_MANUAL; /* 有效遥控始终优先，CH7仍只切塔吊。 */

    /* 塔吊模式(CH7 开关拨动): 底盘完全停止, 交回 TOWER_Task 控制塔吊 */
    ch_mode = crsf_get_channel(REMOTE_CH_MODE);
    if (ch_mode > REMOTE_MODE_THRESHOLD)
    {
      /* 塔吊模式: 底盘停 + 电机6(齿轮齿条)三态速度控制(仅塔吊模式可控, 与底盘驱动分开)。
       *   0191(<600) → 正方向缓速;  0997(中位) → 停止并自锁;  1792(>1300) → 反方向缓速。 */
      chassis_set_target(0.0f, 0.0f, 0.0f);
      {
        uint16_t ch_m6 = crsf_get_channel(REMOTE_CH_MOTOR6);

        if (!motor6_initialized)
        {
          /* 一次性初始化: 使能 + 闭环FOC + 电流上限(6000mA), 周期重发会打断运行致"旋转不连贯" */
          X_V2_En_Control(MOTOR6_ADDR, true, false);
          X_V2_Modify_Ctrl_Mode(MOTOR6_ADDR, false, true);
          X_V2_Modify_FOC_mA(MOTOR6_ADDR, false, MOTOR6_MAX_CURRENT_MA);
          motor6_initialized = 1U;
        }

        if (ch_m6 < MOTOR6_SW_LOW_TH)
        {
          X_V2_Vel_Control(MOTOR6_ADDR, MOTOR6_DIR_FWD, MOTOR6_ACC_RPMPS, MOTOR6_SLOW_RPM, false);
        }
        else if (ch_m6 > MOTOR6_SW_HIGH_TH)
        {
          X_V2_Vel_Control(MOTOR6_ADDR, MOTOR6_DIR_REV, MOTOR6_ACC_RPMPS, MOTOR6_SLOW_RPM, false);
        }
        else
        {
          X_V2_Stop_Now(MOTOR6_ADDR, false);   /* 中位 997: 停止并自锁 */
        }

        /* 读电机6实时转速: X_V2 电机不主动上报, 必须被读才回 0x35 速度帧(OLED 第8行诊断)。 */
        X_V2_Read_Sys_Params(MOTOR6_ADDR, S_VEL);
      }
      osDelay(10);
      continue;
    }

    /* 非塔吊模式: 电机6 停止(防失控), 底盘正常驱动 */
    if (motor6_initialized) { X_V2_Stop_Now(MOTOR6_ADDR, false); }

    ch_thr = crsf_get_channel(REMOTE_CH_THROTTLE);
    ch_fwd = crsf_get_channel(REMOTE_CH_FORWARD);
    ch_str = crsf_get_channel(REMOTE_CH_STRAFE);
    ch_rot = crsf_get_channel(REMOTE_CH_ROTATE);

    REMOTE_THROTTLE_NORM(ch_thr, thr);
    REMOTE_NORM(ch_fwd, nvx);
    REMOTE_NORM(ch_str, nvy);
    REMOTE_NORM(ch_rot, nwz);

    /* 油门(全局速度) × 方向; 旋转也随油门, 油门归零全部停止 */
    chassis_set_target(thr * nvx * REMOTE_SIGN_VX * REMOTE_MAX_VX,
                       thr * nvy * REMOTE_SIGN_VY * REMOTE_MAX_VY,
                       thr * nwz * REMOTE_SIGN_WZ * REMOTE_MAX_WZ);

    osDelay(10);
  }
  /* USER CODE END StartTask04 */
}

/* USER CODE BEGIN Header_StartTask05 */
/**
* @brief Function implementing the MAIN_Task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask05 */
void StartTask05(void const * argument)
{
  /* USER CODE BEGIN StartTask05 */
  /* 复用空闲任务：DMA字节缓冲在这里解析，ACK/STATUS只在任务中发送。 */
  for (;;)
  {
    /* USB CDC中断已缓冲字节；不启动USART3接收，避免两个输入源混帧。 */
    pi_link_process();
    osDelay(10);
  }
  /* USER CODE END StartTask05 */
}

/* USER CODE BEGIN Header_StartTask06 */
/**
* @brief Function implementing the TOWER_Task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask06 */
void StartTask06(void const * argument)
{
  /* USER CODE BEGIN StartTask06 */
  /* ── 塔吊遥控任务 ──
   * CH7 开关拨动 → 进入塔吊模式(此时底盘由 StartTask04 完全停止)。
   *   升降: 前进/后退摇杆(CH2 回中 992) → 电机 #5 速度控制(前进=升, 后退=降, 回中=停)。
   *         直接 X_V2_Vel_Control 速度控制, 不依赖位置反馈 → 无积分/无位置环跑飞。
   *   旋转: CH4(原底盘自转摇杆, 回中) → 6020 转速(速度环); 回中=角度环锁死当前位置。
   * 掉线/退出塔吊模式 → 停升降 + 6020 电压归零。 */
  uint16_t ch_mode, ch_fwd, ch_rot;
  float nfwd, nrot;
  float lift_vel_rpm;
  float rot_ref_rpm;
  float ang_err;
  float voltage;
  uint8_t lift_initialized = 0U;
  uint8_t tower_active = 0U;
  float   yaw_unwrapped = 0.0f;   /* 连续(解缠绕)角度, 可超 8191 */
  float   hold_unwrapped = 0.0f;  /* 锁位目标(解缠绕角度) */
  int32_t yaw_prev = 0;           /* 上一拍转子角度, 计算增量用 */
  uint8_t rot_mode_active = 0U;   /* 上一拍是否处于摇杆速度模式(用于切换时复位速度环) */
  pi_target_t target;
  uint8_t auto_grip_color = 0U;
  int16_t grip_last_angle = 0;    /* 夹爪舵机上次写入角度(拨杆换档才更新, 减少写比较寄存器) */

  for (;;)
  {
    if (g_control_mode == CONTROL_MODE_AUTO || !crsf_is_linked(REMOTE_LINK_TIMEOUT_MS))
    {
      /* 自动夹爪由本任务统一控制；蓝色底盘动作在UART_RX_Task，升降/6020停止。 */
      if (lift_initialized) { X_V2_Stop_Now(TOWER_LIFT_MOTOR_ADDR, false); }
      set_GM6020_motor_voltage(&hcan1, 0);
      g_6020_voltage_cmd = 0;
      g_tower_mode_active = 0U;
      tower_active = 0U;
      taskENTER_CRITICAL();
      pi_link_get_target(&target);
      if (!target.valid || (target.color != 1U && target.color != 4U))
      {
        auto_grip_color = 0U;
        g_auto_red_grab_done = 0U;
      }
      /* 红色闭合、绿色张开；同色目标只触发一次，手动链路恢复即停止自动操作。 */
      if (g_control_mode == CONTROL_MODE_AUTO && !crsf_is_linked(REMOTE_LINK_TIMEOUT_MS) &&
          target.valid && (target.color == 1U || target.color == 4U))
      {
        if (auto_grip_color != target.color)
        {
          grip_last_angle = (target.color == 1U) ? TOWER_GRIP_ANGLE_LOW : TOWER_GRIP_ANGLE_HIGH;
          SG90_SetAngle(grip_last_angle);
          auto_grip_color = target.color;
        }
        g_auto_red_grab_done = (target.color == 1U) ? 1U : 0U;
      }
      taskEXIT_CRITICAL();
      osDelay(10);
      continue;
    }
    g_auto_red_grab_done = 0U;
    auto_grip_color = 0U;

    ch_mode = crsf_get_channel(REMOTE_CH_MODE);

    if (ch_mode <= REMOTE_MODE_THRESHOLD)
    {
      /* 非塔吊模式: 停执行器(升降/6020) */
      if (lift_initialized)  { X_V2_Stop_Now(TOWER_LIFT_MOTOR_ADDR, false); }
      set_GM6020_motor_voltage(&hcan1, 0);
      g_tower_mode_active = 0U;
      tower_active = 0U;
      osDelay(10);
      continue;
    }

    /* ── 塔吊模式 ── */
    if (!lift_initialized)
    {
      /* 首次进入: 使能升降电机 #5 */
      X_V2_En_Control(TOWER_LIFT_MOTOR_ADDR, true, false);
      lift_initialized = 1U;
    }

    if (!tower_active)
    {
      /* 刚进入塔吊模式: 用当前 6020 角度作为回中锁位目标(解缠绕角度) */
      yaw_unwrapped = (float)motor_yaw_info.rotor_angle;
      yaw_prev      = (int32_t)motor_yaw_info.rotor_angle;
      hold_unwrapped = yaw_unwrapped;
      pid_reset(&gimbal_yaw_speed_pid);
      rot_mode_active = 0U;
      g_tower_mode_active = 1U;
      tower_active = 1U;
    }

    ch_fwd = crsf_get_channel(REMOTE_CH_FORWARD);
    ch_rot = crsf_get_channel(REMOTE_CH_ROTATE);

    /* 升降: 前进/后退摇杆 → 电机 #5 速度 (前进=升, 后退=降, 回中=停) */
    REMOTE_NORM(ch_fwd, nfwd);
    lift_vel_rpm = nfwd * TOWER_LIFT_MAX_RPM;

    if (lift_vel_rpm > 1.0f)
    {
      X_V2_Vel_Control(TOWER_LIFT_MOTOR_ADDR, TOWER_LIFT_DIR_UP, TOWER_LIFT_ACC_RPMPS, lift_vel_rpm, false);
    }
    else if (lift_vel_rpm < -1.0f)
    {
      X_V2_Vel_Control(TOWER_LIFT_MOTOR_ADDR, TOWER_LIFT_DIR_DOWN, TOWER_LIFT_ACC_RPMPS, -lift_vel_rpm, false);
    }
    else
    {
      /* 回中: 急停立即刹住。速度控制的 acc 同时是减速斜率, 回中发 vel=0 会按 acc 继续滑行减速 */
      X_V2_Stop_Now(TOWER_LIFT_MOTOR_ADDR, false);
    }

    /* 旋转: CH4 → 6020。偏离回中=速度环跟随; 回中=角度环锁定当前位置 */
    REMOTE_NORM(ch_rot, nrot);

    /* 解缠绕: 累计角度增量, 正确处理 0/8191 跨越。
     * 锁位误差用连续角度 → 推过 180° 也沿"原路返回", 不会突然反向(刚度突降)。 */
    {
      int32_t cur   = (int32_t)motor_yaw_info.rotor_angle;
      int32_t delta = cur - yaw_prev;
      if (delta > 4096)       { delta -= 8192; }
      else if (delta < -4096) { delta += 8192; }
      yaw_unwrapped += (float)delta;
      yaw_prev = cur;
    }

    /* 模式切换(锁位↔摇杆)时复位速度环, 避免旧误差/微分记忆在切换瞬间打冲击 → 短暂失控震荡 */
    {
      uint8_t rot_active = (nrot > 0.01f || nrot < -0.01f) ? 1U : 0U;
      if (rot_active != rot_mode_active)
      {
        pid_reset(&gimbal_yaw_speed_pid);
        rot_mode_active = rot_active;
      }
    }

    if (nrot > 0.01f || nrot < -0.01f)
    {
      /* 摇杆偏离: 速度模式, 并同步把锁位目标更新到当前角度(松杆就锁在这里) */
      rot_ref_rpm = nrot * TOWER_ROTATE_SIGN * TOWER_ROTATE_MAX_RPM;
      voltage = pid_calc(&gimbal_yaw_speed_pid, rot_ref_rpm, (float)motor_yaw_info.rotor_speed);
      hold_unwrapped = yaw_unwrapped;
    }
    else
    {
      /* 回中: 级联锁位。外环 位置误差→转速指令, 内环复用速度环(自带阻尼, 不易震荡)。
       * 位置误差用解缠绕角度(无 180° 翻转)。 */
      ang_err = hold_unwrapped - yaw_unwrapped;

      if (ang_err > TOWER_HOLD_ERR_MAX)       { ang_err = TOWER_HOLD_ERR_MAX; }
      else if (ang_err < -TOWER_HOLD_ERR_MAX) { ang_err = -TOWER_HOLD_ERR_MAX; }

      /* 外环速度阻尼: 用实测转速直接压低转速指令, 额外吸振(拨动激振/来回摆) */
      rot_ref_rpm = TOWER_HOLD_ANGLE_KP * ang_err - TOWER_HOLD_DAMP * (float)motor_yaw_info.rotor_speed;
      if (rot_ref_rpm > TOWER_HOLD_MAX_SPEED)       { rot_ref_rpm = TOWER_HOLD_MAX_SPEED; }
      else if (rot_ref_rpm < -TOWER_HOLD_MAX_SPEED) { rot_ref_rpm = -TOWER_HOLD_MAX_SPEED; }

      voltage = pid_calc(&gimbal_yaw_speed_pid, rot_ref_rpm, (float)motor_yaw_info.rotor_speed);
    }

    g_6020_voltage_cmd = (int16_t)voltage;   /* 诊断: 记录本拍 6020 电压指令 */
    set_GM6020_motor_voltage(&hcan1, (int16_t)voltage);

    /* 夹爪舵机: CH5(通道5) 三段拨杆 → 三档角度。只在换档时写比较寄存器, 避免每拍抖动。 */
    {
      uint16_t ch_grip = crsf_get_channel(TOWER_GRIP_CH_INDEX);
      int16_t  grip_angle;

      if (ch_grip < TOWER_GRIP_LOW_TH)       { grip_angle = TOWER_GRIP_ANGLE_LOW; }
      else if (ch_grip > TOWER_GRIP_HIGH_TH) { grip_angle = TOWER_GRIP_ANGLE_HIGH; }
      else                                   { grip_angle = TOWER_GRIP_ANGLE_MID; }

      if (grip_angle != grip_last_angle)
      {
        SG90_SetAngle(grip_angle);
        grip_last_angle = grip_angle;
      }
    }

    osDelay(10);
  }
  /* USER CODE END StartTask06 */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* ── GM6020 云台辅助函数（对齐 F407VET6） ─────────────────── */

/* GM6020 辅助函数 — 暂时禁用，恢复 TOWER_Task 时解开（下方 #if 0 到 #endif 全部解开） */
#if 0

/* 保持当前位置一段时间的 PID 控制（阻塞式） */
static void gimbal_hold_control(uint32_t ms)
{
  uint32_t start = HAL_GetTick();
  while ((HAL_GetTick() - start) < ms)
  {
    int16_t yaw_voltage = gimbal_update(motor_yaw_info.rotor_angle,
                                        motor_yaw_info.rotor_speed);
    set_GM6020_motor_voltage(&hcan1, yaw_voltage);
    osDelay(2);  /* 500Hz */
  }
}

/* 等待到达目标位置，带超时保护（阻塞式） */
static void gimbal_wait_reached(uint32_t timeout_ms)
{
  uint32_t start = HAL_GetTick();
  while ((HAL_GetTick() - start) < timeout_ms)
  {
    int16_t yaw_voltage = gimbal_update(motor_yaw_info.rotor_angle,
                                        motor_yaw_info.rotor_speed);
    set_GM6020_motor_voltage(&hcan1, yaw_voltage);

    if (gimbal_is_target_reached(motor_yaw_info.rotor_angle,
                                 motor_yaw_info.rotor_speed))
    {
      gimbal_hold_control(200U);  /* 到达稳住 200ms 防过冲 */
      return;
    }
    osDelay(2);  /* 500Hz */
  }
}

#endif /* #if 0 — GM6020 辅助函数 */

/* USER CODE END Application */
