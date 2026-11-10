#include "stm32f1xx_hal.h"

/* External variable from usbd_conf.c */
extern PCD_HandleTypeDef hpcd_USB_FS;

/******************************************************************************/
/*           Cortex-M3 Processor Interruption and Exception Handlers          */
/******************************************************************************/
void NMI_Handler(void) {}

void HardFault_Handler(void) {
  while (1) {}
}

void MemManage_Handler(void) {
  while (1) {}
}

void BusFault_Handler(void) {
  while (1) {}
}

void UsageFault_Handler(void) {
  while (1) {}
}

void SVC_Handler(void) {}

void DebugMon_Handler(void) {}

void PendSV_Handler(void) {}

/* 
 * CRITICAL: SysTick_Handler is required for HAL_Delay() to work.
 * Without this, the code will hang at HAL_Delay(50) in main.c
 * and USB will never initialize!
 */
void SysTick_Handler(void) {
  HAL_IncTick();
}

/******************************************************************************/
/* STM32F1xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32f103xb.s).                  */
/******************************************************************************/

/* 
 * CRITICAL: USB Low Priority Interrupt Handler.
 * Without this, the MCU will not respond to PC's SETUP packets (Code 43).
 */
void USB_LP_CAN1_RX0_IRQHandler(void) {
  HAL_PCD_IRQHandler(&hpcd_USB_FS);
}

/* Optional: USB Wakeup Interrupt */
void USBWakeUp_IRQHandler(void) {
  __HAL_USB_WAKEUP_EXTI_CLEAR_FLAG();
}