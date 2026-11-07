#ifndef CPI_USBD_CONF_H
#define CPI_USBD_CONF_H
#include "stm32f4xx_hal.h"
#include <string.h>
#define USE_USB_FS
#define USBD_MAX_NUM_INTERFACES 2U
#define USBD_MAX_NUM_CONFIGURATION 1U
#define USBD_MAX_STR_DESC_SIZ 256U
#define USBD_SELF_POWERED 1U
#define USBD_DEBUG_LEVEL 0U
#define USBD_LPM_ENABLED 0U
#define USBD_MAX_SUPPORTED_CLASS 1U
void *USB_CPI_Alloc(uint32_t size);
void USB_CPI_Free(void *ptr);
#define USBD_malloc USB_CPI_Alloc
#define USBD_free USB_CPI_Free
#define USBD_memset memset
#define USBD_memcpy memcpy
#define USBD_Delay HAL_Delay
#define USBD_UsrLog(...)
#define USBD_ErrLog(...)
#define USBD_DbgLog(...)
#endif
