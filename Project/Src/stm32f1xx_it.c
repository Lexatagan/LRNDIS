/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"
#include "stm32f1xx.h"
#include "stm32f1xx_it.h"
#include "bsp_driver_sd.h"

/* External variables --------------------------------------------------------*/
extern PCD_HandleTypeDef hpcd_USB_FS;
extern uint32_t XXX;    //TODO
/******************************************************************************/
/*            Cortex-M3 Processor Interruption and Exception Handlers         */ 
/******************************************************************************/

/* This function handles System tick timer. */
void SysTick_Handler(void)
{
  HAL_IncTick();
  HAL_SYSTICK_IRQHandler();
}

/* This function handles DMA SDIO interrupts. */
void DMA2_Channel4_5_IRQHandler(void)
{
  BSP_SD_DMA_Tx_IRQHandler();
}

/* This function handles SDIO interrupts. */
void SDIO_IRQHandler(void)
{
  /* Process All SDIO Interrupt Sources */
  BSP_SD_IRQHandler();
}

/******************************************************************************/
/* STM32F1xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32f1xx.s).                    */
/******************************************************************************/

/* This function handles USB low priority or CAN RX0 interrupts. */
void USB_LP_CAN1_RX0_IRQHandler(void)
{
  HAL_PCD_IRQHandler(&hpcd_USB_FS);
}
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
