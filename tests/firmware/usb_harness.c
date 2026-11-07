/* Compile usb_cpi.c's actual body against deterministic USB/RTOS stubs. */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define USBD_OK 0
#define USBD_STATE_CONFIGURED 3
#define CDC_SET_LINE_CODING 0x20
#define CDC_GET_LINE_CODING 0x21
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
typedef struct {uint32_t TxState;} USBD_CDC_HandleTypeDef;
typedef struct {void *pClassData;uint8_t dev_state;} USBD_HandleTypeDef;
typedef struct {int8_t(*Init)(void);int8_t(*DeInit)(void);int8_t(*Control)(uint8_t,uint8_t*,uint16_t);int8_t(*Receive)(uint8_t*,uint32_t*);int8_t(*TransmitCplt)(uint8_t*,uint32_t*,uint8_t);} USBD_CDC_ItfTypeDef;
static int VCP_Desc,USBD_CDC;
static USBD_CDC_HandleTypeDef cls;
static uint32_t ticks;
static uint8_t *txptr,*rxptr,received[64];
static unsigned sent,received_n,rearmed,errors;
static int complete_on_delay;
uint32_t HAL_GetTick(void){return ticks;}
static void Error_Handler(void){++errors;}
static void osDelay(int n){ticks+=(uint32_t)n;if(complete_on_delay)cls.TxState=0;}
static void pi_link_rx_error_isr(void){++errors;}
static void pi_link_rx_byte_isr(uint8_t b){received[received_n++]=b;}
static int USBD_CDC_SetTxBuffer(USBD_HandleTypeDef*d,uint8_t*b,uint32_t n){(void)d;(void)n;txptr=b;return 0;}
static int USBD_CDC_SetRxBuffer(USBD_HandleTypeDef*d,uint8_t*b){(void)d;rxptr=b;return 0;}
static int USBD_CDC_TransmitPacket(USBD_HandleTypeDef*d){(void)d;cls.TxState=1;++sent;return 0;}
static int USBD_CDC_ReceivePacket(USBD_HandleTypeDef*d){(void)d;++rearmed;return 0;}
static int USBD_Init(USBD_HandleTypeDef*d,void*p,int n){(void)p;(void)n;d->pClassData=&cls;d->dev_state=3;return 0;}
static int USBD_RegisterClass(USBD_HandleTypeDef*d,void*p){(void)d;(void)p;return 0;}
static int USBD_CDC_RegisterInterface(USBD_HandleTypeDef*d,USBD_CDC_ItfTypeDef*p){(void)d;return p->Init();}
static int USBD_Start(USBD_HandleTypeDef*d){(void)d;return 0;}
#include "usb_body.inc"
int test_usb_tx_lifetime(void){
    uint8_t a[]={1,2,3},b[]={4,5,6};
    cls.TxState=0;ticks=sent=0;complete_on_delay=0;USB_CPI_Init();
    if(USB_CPI_Send(a,3)!=0)return 1;
    a[0]=99;if(txptr[0]!=1)return 2;
    if(USB_CPI_Send(b,3)==0||txptr[0]!=1||sent!=1||ticks!=10)return 3;
    complete_on_delay=1;if(USB_CPI_Send(b,3)!=0||sent!=2||txptr[0]!=4)return 4;
    hUsbDeviceFS.dev_state=0;if(USB_CPI_Send(a,3)==0||txptr[0]!=4)return 5;
    return 0;
}
int test_usb_rx_and_control(void){
    uint8_t data[]={0xa5,0x5a,1,2,3},line[]={0x80,0x25,0,0,0,0,8},out[7]={0};uint32_t n=5;
    received_n=rearmed=0;USB_CPI_Init();cdc_receive(data,&n);
    if(received_n!=5||memcmp(received,data,5)||rearmed!=1||rxptr!=(uint8_t*)rx_memory)return 1;
    cdc_control(CDC_SET_LINE_CODING,line,7);cdc_control(CDC_GET_LINE_CODING,out,7);
    return memcmp(line,out,7)!=0;
}
int test_usb_static_allocation(void){
    uint8_t*p=(uint8_t*)USB_CPI_Alloc(1);if(!p)return 1;p[0]=99;
    if(USB_CPI_Alloc(1)!=p||p[0]!=0)return 2;
    return USB_CPI_Alloc(sizeof(class_memory)+1U)!=NULL;
}
