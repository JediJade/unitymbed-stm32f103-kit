/*
 * Blink, bare metal - the starting point for a new STM32F103 project.
 *
 * Pins (Blue Pill):
 *   PC13 - on-board LED, active low, toggles every 500 ms
 *
 * Nothing but registers: 72 MHz from the 8 MHz crystal, a 1 ms SysTick, and
 * one GPIO. clock_init() and the SysTick/delay code are copied unchanged from
 * the hardware-verified USB templates, so a project that starts here can add
 * USB later without touching the clock tree (USB needs exactly this 72 MHz
 * setup, because 48 MHz comes from PLL / 1.5).
 */

#include "stm32f103xb.h"
#include "board.h"

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
  RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;

  /* PC13: output push-pull 2 MHz (CNF=00, MODE=10) */
  GPIOC->CRH &= ~(0xFUL << (4U * (LED_PIN - 8U)));
  GPIOC->CRH |=  (0x2UL << (4U * (LED_PIN - 8U)));
  GPIOC->BSRR = (1UL << LED_PIN);          /* LED off */
}

int main(void)
{
  clock_init();
  SysTick_Config(72000000U / 1000U);   /* 1 ms tick */
  gpio_init();

  while (1) {
    GPIOC->ODR ^= (1UL << LED_PIN);
    delay_ms(500U);
  }
}
