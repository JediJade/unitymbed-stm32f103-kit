#include "stm32f1xx_hal.h"
#include "usb_device.h"
#include "usbd_hid_if.h"

/* HID demo pins (Blue Pill):
     PB0  - push button to GND: hold to move the mouse
     PB1  - push button to GND: tap to type a string
     PC13 - on-board LED, lit once the host has configured the device  */
#define HID_BTN_MOUSE_PIN   GPIO_PIN_0
#define HID_BTN_KBD_PIN     GPIO_PIN_1
#define HID_LED_PIN         GPIO_PIN_13

void SystemClock_Config(void);
void Error_Handler(void);
static void HID_Demo_GPIO_Init(void);

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

  HID_Demo_GPIO_Init();
  MX_USB_DEVICE_Init();

  uint8_t  kbd_btn_prev = 1U;   /* buttons are active low */
  uint16_t tick = 0U;

  while (1)
  {
    uint8_t configured = HID_Ready();

    /* LED on PC13 is active low on the Blue Pill */
    HAL_GPIO_WritePin(GPIOC, HID_LED_PIN,
                      configured ? GPIO_PIN_RESET : GPIO_PIN_SET);

    /* --- PB0 held: walk the pointer right, then back left --- */
    if (HAL_GPIO_ReadPin(GPIOB, HID_BTN_MOUSE_PIN) == GPIO_PIN_RESET)
    {
      int8_t dx = (tick < 25U) ? 8 : -8;
      (void)HID_Mouse_Send(dx, 0, 0, 0U);
      tick = (uint16_t)((tick + 1U) % 50U);
    }
    else
    {
      tick = 0U;
    }

    /* --- PB1 pressed: type one line --- */
    uint8_t kbd_btn = (HAL_GPIO_ReadPin(GPIOB, HID_BTN_KBD_PIN) == GPIO_PIN_SET) ? 1U : 0U;

    if ((kbd_btn == 0U) && (kbd_btn_prev == 1U))
    {
      HAL_Delay(20);  /* debounce */
      if (HAL_GPIO_ReadPin(GPIOB, HID_BTN_KBD_PIN) == GPIO_PIN_RESET)
      {
        HID_Keyboard_TypeString("Hello from STM32 HID!\n");
      }
    }
    kbd_btn_prev = kbd_btn;

    /* Matches the 10 ms endpoint polling interval */
    HAL_Delay(HID_FS_BINTERVAL);
  }
}

static void HID_Demo_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /* Buttons: internal pull-up, so an unconnected pin reads HIGH = idle */
  GPIO_InitStruct.Pin  = HID_BTN_MOUSE_PIN | HID_BTN_KBD_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* Status LED */
  GPIO_InitStruct.Pin   = HID_LED_PIN;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOC, HID_LED_PIN, GPIO_PIN_SET);  /* off */
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
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

void Error_Handler(void)
{
  while(1) {}
}
