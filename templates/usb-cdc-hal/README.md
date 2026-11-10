# STM32F103 USB CDC (virtual COM port) - HAL + ST USB library

A virtual COM port built on ST's HAL and the STM32 USB device library (CDC
class), streaming ADC readings from PA0. Same structure CubeMX generates, so it
is the template to start from when you want ST's APIs rather than registers.

| file | role |
| --- | --- |
| `src/main.c` | clocks, ADC on PA0, sends readings over the COM port |
| `src/usbd_cdc_if.c`, `inc/usbd_cdc_if.h` | CDC application callbacks: receive, transmit, line coding |
| `src/usbd_desc.c` | device + string descriptors (VID 0x0483, PID 0x5740) |
| `src/usbd_conf.c` | HAL glue, PMA layout |
| `src/usb_device.c` | library init and CDC class registration |
| `src/stm32f1xx_it.c` | SysTick + USB interrupt handlers |
| `inc/stm32f1xx_hal_conf.h` | which HAL modules are compiled (RCC, GPIO, DMA, FLASH, ADC, PCD) |

The three vendor trees hold only what this firmware compiles, plus their
licence files: 67 files in this template (this README included), against 377
in the original project with the full checkouts.

- `stm32f1xx-hal-driver-master/` - 13 HAL sources and their headers
- `stm32-mw-usb-device-master/` - `Core` and `Class/CDC` only
- `cmsis-device-f1-master/` - `stm32f103xb.h`, `system_stm32f1xx.c`, `startup_stm32f103xb.s`

To add a HAL peripheral, copy its `.c`/`.h` from a fresh
[stm32f1xx-hal-driver](https://github.com/STMicroelectronics/stm32f1xx-hal-driver)
checkout, add the `.c` to `HAL_SRCS` in the `Makefile`, and enable its
`HAL_*_MODULE_ENABLED` in `inc/stm32f1xx_hal_conf.h`.

## Build and flash

```sh
make PROJECT=usb-cdc-hal
make flash PROJECT=usb-cdc-hal
```

Windows 10/11 binds PID 0x5740 to the built-in `usbser.sys`. Read it with:

```powershell
..\..\tools\serial-sniffer.ps1
```

## Status

Trimmed from a project that was verified on hardware. The trimmed template
builds to a `.bin` **byte-identical** to that original (sha256 prefix
`05996ecf2010972d`, arm-none-eabi-gcc 14.2.1 from UnityMbed), so it is the same
firmware.

## Before trusting a build: check `linker.ld`

`unitymbed init` rewrites `linker.ld` from the IDE's own template, which has
used `.vectors` as the vector-table section. ST's startup file emits
`.isr_vector`, and with `--gc-sections` the whole table is then discarded: the
build succeeds, flashing verifies, and the chip locks up before `main()`. A
Blue Pill still shows up on USB because of its D+ pull-up, so it looks like a
USB bug ("Unknown USB Device", code 43). The `linker.ld` here keeps both names.
After any `unitymbed init`, run `sh ../../tools/build-all.sh` from the kit, or:

```sh
grep -q isr_vector linker.ld && echo "linker OK" || echo "linker BROKEN"
od -A x -t x4 -N 4 build/*.bin   # word 0 must be 0x2000xxxx
```
