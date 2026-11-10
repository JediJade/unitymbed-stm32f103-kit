/**
 * stm32f1xx_hal_conf.h - adapted from ST's official
 * stm32f1xx-hal-driver-master/Inc/stm32f1xx_hal_conf_template.h
 *
 * Only the modules this project actually uses are enabled below. The oscillator
 * values, timeouts and TICK_INT_PRIORITY are NOT optional: the HAL sources
 * reference them directly, so a trimmed-down conf header breaks the build with
 * confusing "undeclared identifier" errors deep inside the driver.
 */
#ifndef __STM32F1xx_HAL_CONF_H
#define __STM32F1xx_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* ########################## Module Selection ############################## */
#define HAL_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_ADC_MODULE_ENABLED
#define HAL_PCD_MODULE_ENABLED

/* ########################## Oscillator Values ############################# */
#if !defined  (HSE_VALUE)
  #define HSE_VALUE               8000000U   /* 8 MHz crystal on the Blue Pill */
#endif
#if !defined  (HSE_STARTUP_TIMEOUT)
  #define HSE_STARTUP_TIMEOUT     100U       /* ms */
#endif
#if !defined  (HSI_VALUE)
  #define HSI_VALUE               8000000U
#endif
#if !defined  (LSI_VALUE)
  #define LSI_VALUE               40000U
#endif
#if !defined  (LSE_VALUE)
  #define LSE_VALUE               32768U
#endif
#if !defined  (LSE_STARTUP_TIMEOUT)
  #define LSE_STARTUP_TIMEOUT     5000U      /* ms */
#endif

/* ########################### System Configuration ######################### */
#define  VDD_VALUE                3300U      /* mV */
#define  TICK_INT_PRIORITY        0x0FU      /* lowest priority, SysTick */
#define  USE_RTOS                 0U
#define  PREFETCH_ENABLE          1U

#define  USE_HAL_ADC_REGISTER_CALLBACKS   0U
#define  USE_HAL_PCD_REGISTER_CALLBACKS   0U

/* ########################## Assert Selection ############################## */
#define assert_param(expr) ((void)0U)

/* Includes - DMA must precede ADC (adc.h references DMA_HandleTypeDef),
   and this is the same order ST's own template uses. */
#include "stm32f1xx_hal_rcc.h"
#include "stm32f1xx_hal_gpio.h"
#include "stm32f1xx_hal_dma.h"
#include "stm32f1xx_hal_cortex.h"
#include "stm32f1xx_hal_adc.h"
#include "stm32f1xx_hal_flash.h"
#include "stm32f1xx_hal_pcd.h"

#ifdef __cplusplus
}
#endif

#endif /* __STM32F1xx_HAL_CONF_H */
