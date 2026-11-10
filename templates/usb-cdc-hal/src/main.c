#include "stm32f1xx_hal.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"

ADC_HandleTypeDef hadc1;

void SystemClock_Config(void);
void Error_Handler(void);
void MX_ADC1_Init(void);
uint16_t Format_ADC_String(uint32_t mv, char *buf);

/* Provide missing standard library functions for -nostdlib */
void *memset(void *s, int c, uint32_t n)
{
    unsigned char *p = s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}

void *memcpy(void *dest, const void *src, uint32_t n)
{
    unsigned char *d = dest;
    const unsigned char *s = src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

/* Dummy __libc_init_array for startup file without stdlib */
void __libc_init_array(void)
{
}

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  
  /* Force USB Re-enumeration (Pull PA12 LOW briefly) */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
  HAL_Delay(100); /* Wait 100ms to let PC detect disconnect */
  
  /* IMPORTANT: Set PA12 back to INPUT so the USB peripheral can use it */
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  HAL_Delay(10);

  /* Initialize USB and ADC */
  MX_USB_DEVICE_Init();
  MX_ADC1_Init();

  char tx_buf[32];

  while (1)
  {
    /* Start ADC Conversion */
    HAL_ADC_Start(&hadc1);
    
    /* Wait for conversion to complete (timeout 10ms) */
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
    {
      /* Read raw value (0 - 4095) */
      uint32_t raw_val = HAL_ADC_GetValue(&hadc1);
      
      /* Convert to millivolts (0 - 3300 mV) */
      uint32_t mv_val = (raw_val * 3300) / 4095;
      
      /* Format string and send over USB CDC */
      uint16_t len = Format_ADC_String(mv_val, tx_buf);
      CDC_Transmit_FS((uint8_t*)tx_buf, len);
    }
    
    /* Delay 100ms before next reading */
    HAL_Delay(100);
  }
}

/* Custom function to format ADC value to string (avoids using sprintf/stdlib) */
uint16_t Format_ADC_String(uint32_t mv, char *buf)
{
  char temp[10];
  int i = 0;
  
  /* Convert integer to reversed string */
  if (mv == 0) {
    temp[i++] = '0';
  } else {
    while (mv > 0) {
      temp[i++] = (mv % 10) + '0';
      mv /= 10;
    }
  }
  
  /* Build final string: "ADC0: XXXX mV\r\n" */
  int j = 0;
  buf[j++] = 'A'; buf[j++] = 'D'; buf[j++] = 'C'; buf[j++] = '0'; buf[j++] = ':'; buf[j++] = ' ';
  
  /* Reverse the number back */
  while (i > 0) {
    buf[j++] = temp[--i];
  }
  
  buf[j++] = ' '; buf[j++] = 'm'; buf[j++] = 'V'; buf[j++] = '\r'; buf[j++] = '\n';
  
  return j; /* Return length of the string */
}

void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  /* Enable ADC1 Clock */
  __HAL_RCC_ADC1_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* Configure PA0 as Analog Input */
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* Configure ADC1 */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /* Run ADC Calibration (Crucial for STM32F1 accuracy) */
  HAL_ADCEx_Calibration_Start(&hadc1);

  /* Configure ADC Channel 0 (PA0) */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /* Configure HSE and PLL to reach 72MHz */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9; // 8MHz * 9 = 72MHz
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /* Configure SYSCLK, AHB, APB1, APB2 */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2; // 36MHz max for APB1
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1; // 72MHz max for APB2

  /* 72MHz requires FLASH_LATENCY_2 */
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }

  /* Configure USB Clock to 48MHz (72MHz / 1.5 = 48MHz) */
  /* Configure ADC Clock to 12MHz (72MHz / 6 = 12MHz, max is 14MHz) */
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB | RCC_PERIPHCLK_ADC;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5; 
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

void Error_Handler(void)
{
  while(1) {}
}