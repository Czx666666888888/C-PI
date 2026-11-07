/* Hardware stubs; actual parser and task bodies are compiled below. */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <setjmp.h>
#include "pi_link.h"
#include "usart.h"
UART_HandleTypeDef huart3;
volatile control_mode_t g_control_mode;
volatile uint8_t g_auto_red_grab_done;
static uint32_t tick;
static uint8_t output[8192];
static unsigned output_len;
static int tx_fail;
uint32_t HAL_GetTick(void){return tick;}
int HAL_UART_Transmit(UART_HandleTypeDef*h,uint8_t*p,uint16_t n,uint32_t t){
    (void)h;(void)t;if(tx_fail)return 1;
    if(output_len+n<=sizeof(output)){memcpy(output+output_len,p,n);output_len+=n;}return 0;
}
#include "pi_link.c"
static jmp_buf escape;
static int step,script,linked,failed,grip_calls,zeros,angle;
#define CRSF_CH_CENTER 992
#define hcan1 huart3
#define S_VEL 0
#define crsf_is_linked(timeout) (linked)
static uint16_t crsf_get_channel(int ch){return (script==2 && ch==6) ? 1700 : 992;}
static void chassis_set_target(float x,float y,float z){if(x==0&&y==0&&z==0)++zeros;}
static void X_V2_Stop_Now(int a,int b){(void)a;(void)b;}
static void X_V2_En_Control(int a,int b,int c){(void)a;(void)b;(void)c;}
static void X_V2_Modify_Ctrl_Mode(int a,int b,int c){(void)a;(void)b;(void)c;}
static void X_V2_Modify_FOC_mA(int a,int b,int c){(void)a;(void)b;(void)c;}
static void X_V2_Read_Sys_Params(int a,int b){(void)a;(void)b;}
static void X_V2_Vel_Control(int a,int b,int c,float d,int e){(void)a;(void)b;(void)c;(void)d;(void)e;}
static void set_GM6020_motor_voltage(void*p,int v){(void)p;if(v!=0)failed=1;}
static void SG90_SetAngle(int v){angle=v;++grip_calls;}
typedef int pid_struct_t;
static pid_struct_t gimbal_yaw_speed_pid;
static void pid_reset(void*p){(void)p;}
static float pid_calc(void*p,float a,float b){(void)p;return a-b;}
static struct{int rotor_angle,rotor_speed;}motor_yaw_info;
static uint8_t g_tower_mode_active;
static int16_t g_6020_voltage_cmd;
static void target(int color,int valid){g_pi_target.color=color;g_pi_target.valid=valid;g_pi_target.age_ms=0;g_pi_target.last_rx_tick=tick;}
static void osDelay(int ms){
    (void)ms;
    if(script==1){
        if(step==0&&g_control_mode!=CONTROL_MODE_MANUAL)failed=1;
        if(step>=1&&step<=3&&(g_control_mode!=CONTROL_MODE_AUTO||zeros!=2))failed=1;
        if(step==4&&g_control_mode!=CONTROL_MODE_MANUAL)failed=1;
        if(++step==5)longjmp(escape,1);
        linked=(step==4);
    }else{
        static const int expected[]={1,1,1,1,1,2,3,3,3,4,4,5};
        if(grip_calls!=expected[step])failed=1;
        if(step==6&&angle!=-40)failed=1; /* original CH4 center -> half open */
        if(++step==12)longjmp(escape,1);
        if(step==2)target(4,1);
        if(step==3)target(3,1);
        if(step==4)target(0,0);
        if(step==5)target(1,1);
        if(step==6){g_control_mode=CONTROL_MODE_MANUAL;linked=1;}
        if(step==7){g_control_mode=CONTROL_MODE_AUTO;linked=1;}
        if(step==8){linked=0;target(1,1);g_pi_target.age_ms=501;}
        if(step==9)target(1,1);
        if(step==10)tick+=501;
        if(step==11)target(1,1);
    }
}
#include "tasks.inc"
void test_reset(void){rx_head=rx_tail=rx_fault=used=heartbeat_seen=0;frame_tick=heartbeat_tick=status_tick=tick=0;tx_seq=0;
    memset((void*)&g_pi_target,0,sizeof(g_pi_target));g_pi_frame_errors=g_pi_rx_errors=g_pi_tx_errors=0;
    g_control_mode=CONTROL_MODE_AUTO;g_auto_red_grab_done=0;output_len=tx_fail=0;}
void test_tick(uint32_t t){tick=t;}
unsigned test_output(uint8_t*p){memcpy(p,output,output_len);unsigned n=output_len;output_len=0;return n;}
uint16_t test_crc(const uint8_t*p,uint8_t n){return crc16(p,n);}
void test_tx_fail(void){tx_fail=1;}
int test_modes(void){step=failed=zeros=0;script=1;linked=1;g_control_mode=CONTROL_MODE_AUTO;if(!setjmp(escape))StartTask04(0);return failed;}
int test_grip(void){step=failed=grip_calls=0;script=2;linked=0;g_control_mode=CONTROL_MODE_AUTO;target(1,1);if(!setjmp(escape))StartTask06(0);return failed;}

