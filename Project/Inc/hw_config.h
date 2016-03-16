/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __HW_CONFIG_H
#define __HW_CONFIG_H

/* Includes ------------------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
typedef struct load_counter
{
  uint32_t cnt;
  uint32_t result;
}cpu_LoadCounterTypeDef;
/* Exported constants --------------------------------------------------------*/

/* NVIC */
#define SYSTICK_PREEMPT_PRIORITY        2       //Low
#define SDIODMA_PREEMPT_PRIORITY        1       //Middle
#define SDIO_PREEMPT_PRIORITY           0       //High

/* USB */
#define USB_DISCONNECT_PORT             GPIOB
#ifdef USE_TE_STM32F103
  #define USB_DISCONNECT_PIN            GPIO_PIN_5
#endif 
#ifdef USE_HY_STM32MINI
  #define USB_DISCONNECT_PIN            GPIO_PIN_7
#endif 

/* SDIODMA */
#define SDIODMA_Channel_IRQn            DMA2_Channel4_5_IRQn
#define __SDIODMA_CLK_ENABLE()          __HAL_RCC_DMA2_CLK_ENABLE()

/* Exported macro ------------------------------------------------------------*/
/* Exported variables --------------------------------------------------------*/
extern cpu_LoadCounterTypeDef cpuLoad;
/* Exported functions ------------------------------------------------------- */

void init_periph(void);
uint32_t sys_now();

#endif /* __HW_CONFIG_H */
