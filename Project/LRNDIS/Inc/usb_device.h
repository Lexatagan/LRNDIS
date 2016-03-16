/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USB_DEVICE_H
#define __USB_DEVICE_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx.h"
#include "stm32f1xx_hal.h"
#include "usbd_def.h"

extern USBD_HandleTypeDef hUsbDeviceFS;

/* USB_Device init function */	
void USB_DEVICE_Init(void);

#endif /* __USB_DEVICE_H */
