#include "usb_cpi.h"
#include "usbd_core.h"
#include "usbd_cdc.h"
#include "usbd_desc.h"
#include "pi_link.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include <string.h>

USBD_HandleTypeDef hUsbDeviceFS;
static uint32_t class_memory[(sizeof(USBD_CDC_HandleTypeDef) + 3U) / 4U];
static uint32_t rx_memory[16], tx_memory[16]; /* 64字节全速包，4字节对齐。 */
static uint8_t line_coding[7] = {0x00U, 0xC2U, 0x01U, 0U, 0U, 0U, 8U};

void *USB_CPI_Alloc(uint32_t size)
{
    if (size > sizeof(class_memory)) return NULL;
    memset(class_memory, 0, sizeof(class_memory));
    return class_memory; /* 单个CDC类静态内存，无中断malloc/RTOS堆依赖。 */
}
void USB_CPI_Free(void *ptr) { (void)ptr; }

static int8_t cdc_init(void)
{
    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, (uint8_t *)tx_memory, 0U);
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, (uint8_t *)rx_memory);
    return USBD_OK;
}
static int8_t cdc_deinit(void)
{
    pi_link_rx_error_isr();
    return USBD_OK;
}
static int8_t cdc_control(uint8_t cmd, uint8_t *buf, uint16_t length)
{
    /* CDC波特率只是主机元数据，不影响USB总线速率。 */
    if (length >= 7U && cmd == CDC_SET_LINE_CODING) memcpy(line_coding, buf, 7U);
    if (length >= 7U && cmd == CDC_GET_LINE_CODING) memcpy(buf, line_coding, 7U);
    return USBD_OK;
}
static int8_t cdc_receive(uint8_t *buf, uint32_t *length)
{
    uint32_t i;
    for (i = 0U; i < *length; ++i) pi_link_rx_byte_isr(buf[i]);
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, (uint8_t *)rx_memory);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return USBD_OK;
}
static int8_t cdc_transmit_done(uint8_t *buf, uint32_t *length, uint8_t ep)
{
    (void)buf; (void)length; (void)ep;
    return USBD_OK;
}
static USBD_CDC_ItfTypeDef cdc_interface = {
    cdc_init, cdc_deinit, cdc_control, cdc_receive, cdc_transmit_done
};

void USB_CPI_Init(void)
{
    if (USBD_Init(&hUsbDeviceFS, &VCP_Desc, 0U) != USBD_OK ||
        USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC) != USBD_OK ||
        USBD_CDC_RegisterInterface(&hUsbDeviceFS, &cdc_interface) != USBD_OK ||
        USBD_Start(&hUsbDeviceFS) != USBD_OK) Error_Handler();
}

int USB_CPI_Send(const uint8_t *data, uint16_t length)
{
    uint32_t start = HAL_GetTick();
    if (length > sizeof(tx_memory)) return 1;
    for (;;) {
        USBD_CDC_HandleTypeDef *cdc;
        uint8_t result;
        taskENTER_CRITICAL();
        cdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
        if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED || cdc == NULL) {
            taskEXIT_CRITICAL();
            return 1;
        }
        if (cdc->TxState == 0U) {
            /* 独立持久TX缓冲，异步USB完成前不会引用调用者的栈内帧。 */
            memcpy(tx_memory, data, length);
            USBD_CDC_SetTxBuffer(&hUsbDeviceFS, (uint8_t *)tx_memory, length);
            result = USBD_CDC_TransmitPacket(&hUsbDeviceFS);
            taskEXIT_CRITICAL();
            return result == USBD_OK ? 0 : 1;
        }
        taskEXIT_CRITICAL();
        if ((uint32_t)(HAL_GetTick() - start) >= 10U) return 1;
        osDelay(1); /* 仅MAIN_Task等待，不在USB中断发送或等待。 */
    }
}
