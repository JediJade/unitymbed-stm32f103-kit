/*
 * USB CDC (virtual COM port) echo, bare metal - the smallest useful program
 * on top of src/usb_cdc.c.
 *
 * Pins (Blue Pill):
 *   PC13 - on-board LED, lit once the host has configured the device
 *   PA11/PA12 - USB D-/D+, driven by the USB peripheral itself
 *
 * Open the COM port in any terminal (baud rate does not matter - there is no
 * UART behind it). Every byte typed comes back; Enter prints a new prompt.
 *
 * clock_init(), gpio_init() and usb_force_reenumerate() are copied unchanged
 * from the hardware-verified USB HID bare-metal template.
 */

#include "stm32f103xb.h"
#include "board.h"
#include "usb_cdc.h"

#define LED_PIN          13U     /* PC13, active low */

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
  RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPCEN;

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

/*
 * Send a whole buffer. usb_cdc_write() takes at most one packet (63 bytes,
 * and it says how many it took) and refuses while the previous one is still
 * in flight, so wait for the endpoint and advance by what was accepted - but
 * give up if the host goes away, instead of hanging forever.
 */
static void cdc_send(const uint8_t *data, uint16_t len)
{
  uint32_t t0 = millis();

  while (len > 0U) {
    uint16_t n;

    if (usb_cdc_configured() == 0) {
      return;
    }
    if (usb_cdc_write_ready() == 0) {
      if ((millis() - t0) > 100U) {
        return;                            /* nobody is reading */
      }
      continue;
    }

    n = usb_cdc_write(data, len);
    data += n;
    len   = (uint16_t)(len - n);
    t0    = millis();
  }
}

static void cdc_print(const char *s)
{
  uint16_t n = 0U;

  while (s[n] != '\0') {
    n++;
  }
  cdc_send((const uint8_t *)s, n);
}

int main(void)
{
  int was_configured = 0;

  clock_init();
  SysTick_Config(72000000U / 1000U);   /* 1 ms tick */
  gpio_init();

  usb_force_reenumerate();
  usb_cdc_init();

  while (1) {
    uint8_t c;
    int     configured = usb_cdc_configured();

    /* LED on PC13 is active low */
    if (configured != 0) {
      GPIOC->BSRR = (1UL << (LED_PIN + 16U));   /* on */
    } else {
      GPIOC->BSRR = (1UL << LED_PIN);           /* off */
    }

    if ((configured != 0) && (was_configured == 0)) {
      delay_ms(50U);   /* let the host finish SET_LINE_CODING first */
      cdc_print("\r\nSTM32F103 bare metal CDC - type, and it echoes.\r\n> ");
    }
    was_configured = configured;

    while (usb_cdc_read_byte(&c) != 0) {
      if ((c == '\r') || (c == '\n')) {
        cdc_print("\r\n> ");
      } else {
        cdc_send(&c, 1U);
      }
    }
  }
}
