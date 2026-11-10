# STM32F103 Blink - bare metal

The smallest starting point in the kit: the PC13 LED on a Blue Pill toggles
every 500 ms. Registers only - no HAL, no CMSIS `.c` files, no assembly
startup. About 520 bytes of flash.

| file | role |
| --- | --- |
| `src/startup.c` | vector table in `.isr_vector`, `Reset_Handler` (copies `.data`, zeroes `.bss`) |
| `src/main.c` | 72 MHz from HSE + PLL, 1 ms SysTick, `millis()` / `delay_ms()`, blink loop |
| `inc/board.h` | `millis()` and `delay_ms()` for other files |
| `cmsis/` | `stm32f103xb.h`, `core_cm3.h` and friends - register definitions only |
| `linker.ld` | 64 KB flash / 20 KB RAM, vector table first and `KEEP`ed |
| `openocd.cfg` | ST-Link, SWD, `stm32f1x` flash driver |

`clock_init()` is the same code as in the USB templates, so USB can be added
later without touching the clock tree (USB needs 72 MHz, because its 48 MHz
comes from PLL / 1.5).

## Build and flash

In UnityMbed: open the folder, build, flash.

From Git Bash:

```sh
make PROJECT=blink-bare        # build/blink-bare.elf / .bin / .hex
make flash PROJECT=blink-bare  # openocd program + verify + reset
```

Status: builds with no warnings (`-Wall -Wextra`) and the image passes the
vector-table check in `tools/build-all.sh`. **Not yet run on hardware** - the
clock and SysTick code is the hardware-verified code from `usb-hid-bare`, but
this exact image has not been flashed.
