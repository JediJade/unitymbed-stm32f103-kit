/*
 * Board setup and the demo application.
 *
 * Pins (Blue Pill):
 *   PB0  - button to GND, hold: pointer walks right, then back left
 *   PB1  - button to GND, tap: types a line
 *   PC13 - on-board LED, lit once the host has configured the device
 *   PA11/PA12 - USB D-/D+, driven by the USB peripheral itself
 *
 * Both buttons use the internal pull-up, so with nothing wired the firmware
 * sends no reports and leaves the real mouse and keyboard alone.
 */

#include "stm32f103xb.h"
#include "board.h"
#include "usb.h"
#include "hid.h"

#define LED_PIN          13U     /* PC13, active low */
#define BTN_MOUSE_PIN    0U      /* PB0 */
#define BTN_KBD_PIN      1U      /* PB1 */

static volatile uint32_t tick_ms;

void SysTick_Handler(void)
{
  tick_ms++;
}

uint32_t millis(void)
{
  return tick_ms;
}

void delay_ms(uint32_t ms)
{
  uint32_t start = tick_ms;

  while ((tick_ms - start) < ms) {
  }
}

/* HSE 8 MHz -> PLL x9 -> 72 MHz SYSCLK, APB1 36 MHz, USB 48 MHz (PLL / 1.5) */
static void clock_init(void)
{
  /* 72 MHz needs two flash wait states */
  FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2;

  RCC->CR |= RCC_CR_HSEON;
  while ((RCC->CR & RCC_CR_HSERDY) == 0U) {
  }

  /* USBPRE stays 0, which is the 1.5 divider: 72 / 1.5 = 48 MHz */
  RCC->CFGR = RCC_CFGR_PLLSRC        /* PLL fed from HSE */
            | RCC_CFGR_PLLMULL9      /* 8 MHz * 9        */
            | RCC_CFGR_HPRE_DIV1     /* AHB  72 MHz      */
            | RCC_CFGR_PPRE1_DIV2    /* APB1 36 MHz max  */
            | RCC_CFGR_PPRE2_DIV1;   /* APB2 72 MHz      */

  RCC->CR |= RCC_CR_PLLON;
  while ((RCC->CR & RCC_CR_PLLRDY) == 0U) {
  }

  RCC->CFGR |= RCC_CFGR_SW_PLL;
  while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {
  }
}

static void gpio_init(void)
{
  RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN;

  /* PB0, PB1: input with pull-up/pull-down (CNF=10, MODE=00) */
  GPIOB->CRL &= ~((0xFUL << (4U * BTN_MOUSE_PIN)) | (0xFUL << (4U * BTN_KBD_PIN)));
  GPIOB->CRL |=  ((0x8UL << (4U * BTN_MOUSE_PIN)) | (0x8UL << (4U * BTN_KBD_PIN)));
  /* ODR picks which one: 1 = pull-up, so an unconnected pin reads high */
  GPIOB->BSRR = (1UL << BTN_MOUSE_PIN) | (1UL << BTN_KBD_PIN);

  /* PC13: output push-pull 2 MHz (CNF=00, MODE=10) */
  GPIOC->CRH &= ~(0xFUL << (4U * (LED_PIN - 8U)));
  GPIOC->CRH |=  (0x2UL << (4U * (LED_PIN - 8U)));
  GPIOC->BSRR = (1UL << LED_PIN);          /* LED off */
}

/*
 * Pull D+ low for a moment so the host notices a disconnect and re-enumerates.
 * Without this, a board that was just reflashed over SWD often keeps the old
 * session on the host side and never gets re-enumerated.
 */
static void usb_force_reenumerate(void)
{
  /* PA12 as output push-pull 2 MHz, driven low */
  GPIOA->CRH &= ~(0xFUL << (4U * (12U - 8U)));
  GPIOA->CRH |=  (0x2UL << (4U * (12U - 8U)));
  GPIOA->BSRR = (1UL << (12U + 16U));      /* BR12: reset PA12 */
  delay_ms(100U);

  /* Back to input floating so the USB peripheral owns the pin again */
  GPIOA->CRH &= ~(0xFUL << (4U * (12U - 8U)));
  GPIOA->CRH |=  (0x4UL << (4U * (12U - 8U)));
  delay_ms(10U);
}

static int button_pressed(uint32_t pin)
{
  return ((GPIOB->IDR & (1UL << pin)) == 0U) ? 1 : 0;
}

int main(void)
{
  uint8_t  kbd_prev = 0U;   /* 1 while PB1 is held */
  uint16_t phase = 0U;

  clock_init();
  SysTick_Config(72000000U / 1000U);   /* 1 ms tick */
  gpio_init();

  usb_force_reenumerate();
  usb_init();

  while (1) {
    /* LED on PC13 is active low */
    if (usb_configured() != 0) {
      GPIOC->BSRR = (1UL << (LED_PIN + 16U));   /* on */
    } else {
      GPIOC->BSRR = (1UL << LED_PIN);           /* off */
    }

    /* PB0 held: walk the pointer right for 25 reports, then back left */
    if (button_pressed(BTN_MOUSE_PIN) != 0) {
      int8_t dx = (phase < 25U) ? 8 : -8;
      (void)hid_mouse_send(dx, 0, 0, 0U);
      phase = (uint16_t)((phase + 1U) % 50U);
    } else {
      phase = 0U;
    }

    /* PB1 pressed: type one line, on the falling edge only */
    {
      uint8_t kbd_now = (uint8_t)button_pressed(BTN_KBD_PIN);

      if ((kbd_now != 0U) && (kbd_prev == 0U)) {
        delay_ms(20U);                           /* debounce */
        if (button_pressed(BTN_KBD_PIN) != 0) {
          hid_keyboard_type("Hello from bare metal STM32 HID!\n");
        }
      }
      kbd_prev = kbd_now;
    }

    delay_ms(USB_HID_EP_INTERVAL_MS);
  }
}
