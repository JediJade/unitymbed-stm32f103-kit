# STM32F103 USB HID - bare metal

USB HID keyboard + mouse written straight on the STM32F103 registers. No HAL,
no ST USB device library, no CMSIS `.c` files, no assembly startup. The only
thing borrowed from ST is the register map in `cmsis/` (headers only).

Four source files, ~3.2 KB of flash:

| file | role |
| --- | --- |
| `src/startup.c` | vector table in `.isr_vector`, `Reset_Handler` (copies `.data`, zeroes `.bss`) |
| `src/main.c` | 72 MHz clock from HSE+PLL, SysTick, GPIO, D+ re-enumeration pulse, demo loop |
| `src/usb.c` | USB peripheral driver: PMA, BTABLE, EP0 control transfers, descriptors, EP1 IN |
| `src/hid.c` | report builders, `hid_keyboard_type()` and the US-layout ASCII map |
| `inc/` | `usb.h`, `hid.h`, `board.h` |
| `cmsis/` | `stm32f103xb.h`, `core_cm3.h` and friends - register definitions only |

## Build

In UnityMbed IDE: open the folder and build. `unitymbed.json` marks it as an
STM32F103 medium-density project and the IDE's bundled `make` and
`arm-none-eabi` toolchain handle the rest; it passes `PROJECT=<name>`, so the
output is `build/USBHIDBare.elf` / `.bin` / `.hex`.

From a shell, with `arm-none-eabi-gcc` and `make` on PATH:

```sh
make                 # -> build/firmware.elf / .bin / .hex
make PROJECT=USBHIDBare
make flash           # openocd program + verify + reset
../../tools/hid-sniffer.ps1   # kit tools/: what Windows parsed, and -Live for reports
```

`SRCS` is `$(wildcard src/*.c)`, so a new file in `src/` is picked up with no
Makefile edit - but note that changes the link order and therefore every
function address, even when the code is identical.

## Interface

Identical on the wire to the HAL version, except the product ID:

```
VID 0x0483  PID 0x5751  "STM32 Bare Metal HID"
EP 0x81, interrupt IN, 16 bytes, 10 ms

ID 1  keyboard : modifiers, reserved, keycode[6]   -> 9 bytes
ID 2  mouse    : buttons, X, Y, wheel              -> 5 bytes
```

The 101-byte report descriptor is byte-identical to the one verified on
hardware in the HAL project. The PID differs on purpose: Windows caches
descriptors per VID/PID, so reusing 0x5750 for a different firmware can leave
you debugging a stale cached descriptor.

Measured on hardware: enumerates as a keyboard (9-byte input report) plus a
mouse (5-byte input report), and reports arrive every 10.0 ms.

## Demo pins

- **PB0 → GND** (hold): pointer walks right for 25 reports, then back left
- **PB1 → GND** (tap): types `Hello from bare metal STM32 HID!`
- **PC13**: on-board LED, lit once the host has configured the device

Both buttons use the internal pull-up, so with nothing wired the firmware
sends no reports.

## The three things that bite you at register level

**1. `USB_EPnR` mixes three kinds of bits in one 16-bit register.**
`STAT_RX`, `STAT_TX`, `DTOG_RX` and `DTOG_TX` are *toggle-write*: the hardware
XORs what you write into them, so setting a value means writing
`current ^ wanted`. `CTR_RX` and `CTR_TX` are *rc_w0*: writing 0 clears them,
so every unrelated write has to put 1 there or you silently lose a completed
transfer. A plain `EPR(0) = EP_RX_VALID;` looks right and does the wrong thing
in two different ways. Both rules live in `ep_set_tx_status()` /
`ep_set_rx_status()` in `src/usb.c` - nothing else should assign to `EPR()`
except the initial configuration of a fresh endpoint.

**2. PMA is not flat memory.** The 512-byte packet buffer is seen by the CPU
as 16-bit words in 32-bit slots, so buffer offset `o` is at
`0x40006000 + o*2`, 16-bit accesses only. `COUNT_RX` is not a byte count
either: it encodes the buffer size as 32-byte or 2-byte blocks
(`rx_blocks()`).

**3. `SET_ADDRESS` must not take effect immediately.** The host expects the
status stage of that request to still be answered on address 0. `DADDR` is
written in `ep0_in_done()`, after the zero-length status packet has gone out.

A fourth one that is not about the peripheral: the 101-byte HID report
descriptor does not fit in one 64-byte EP0 packet, so the control IN data
stage has to be chunked (`ctl_send_chunk()`). A stack that only ever served
short descriptors appears to work until the host asks for the report
descriptor, and then enumeration fails.

## ⚠️ `unitymbed init` and the vector table

`unitymbed init` rewrites `linker.ld` from the IDE template, and that template
has historically used the wrong section name for the vector table
(`.vectors`, while the table here is emitted into `.isr_vector`). With
`--gc-sections` the linker then discards the whole table: the build succeeds
with no warning, flashing succeeds and verifies, and the MCU loads a garbage
stack pointer at reset and locks up before `main()`. On a Blue Pill the
hardware D+ pull-up still makes the host see a device, so it looks like a USB
bug ("Unknown USB Device (Device Descriptor Request Failed)", code 43) rather
than a dead chip.

The `linker.ld` in this project puts the table in its own `KEEP`ed
`.isr_vector` section, first in flash. **Re-run these two checks after any
`unitymbed init` or linker script change:**

```sh
arm-none-eabi-nm build/*.elf | grep g_pfnVectors    # must be at 0x08000000
od -A x -t x4 -N 8 build/*.bin                      # word 0 = 0x2000xxxx, word 1 odd (thumb)
```
