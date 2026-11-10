# STM32F103 USB HID (keyboard + mouse)

Bare-metal USB HID device for an STM32F103 (Blue Pill), built on ST's HAL and
the STM32 USB device library. The device exposes one HID interface carrying
two reports, so the host binds it as a keyboard **and** a mouse:

```
ID 1  keyboard : modifiers, reserved, keycode[6]   -> 9 bytes
ID 2  mouse    : buttons, X, Y, wheel              -> 5 bytes
```

Both share one 16-byte interrupt IN endpoint (EP 0x81) polled every 10 ms.
Works standalone as a plain make project; `unitymbed init` probes the board
over ST-Link and rewrites CPU/flash/RAM/openocd target for the part it finds.

## Build and flash

```sh
make                      # -> build/firmware.elf / .bin / .hex
make clean
openocd -f openocd.cfg -c "program build/firmware.hex verify reset exit"
```

## Layout

| file | role |
| --- | --- |
| `src/usbd_hid_if.c` | HID class driver: descriptors, class requests, report helpers |
| `inc/usbd_hid_if.h` | endpoint sizes, report IDs, modifier bits, usage IDs |
| `src/usbd_desc.c` | device + string descriptors (VID 0x0483, PID 0x5750, device class 0x00) |
| `src/usbd_conf.c` | HAL glue, PMA layout (EP0 + one 16 B interrupt IN), static class allocation |
| `src/usb_device.c` | library init and class registration |
| `src/main.c` | clocks, demo application |
| `src/stm32f1xx_it.c` | SysTick + USB interrupt handlers |
| `../../tools/hid-sniffer.ps1` | (in the kit) host-side check: parsed descriptor, and live reports via Raw Input |

The HID class driver is local to `src/`, so nothing from
`stm32-mw-usb-device-master/Class` is compiled - only `Core` (`usbd_core.c`,
`usbd_ctlreq.c`, `usbd_ioreq.c`). Do **not** add
`stm32-mw-usb-device-master/Class/HID/Src/usbd_hid.c` to the build: it exports
the same `USBD_HID` symbol and is mouse-only with 4-byte reports.

The three vendor trees hold only the files this firmware compiles (plus their
licence files); everything else was removed. The CMSIS core and device headers
(`core_cm3.h`, `cmsis_gcc.h`, `stm32f1xx.h`, `stm32f103xb.h`,
`system_stm32f1xx.h`, ...) live at the project root and are picked up through
`-I.` - note they are an older revision than the headers that shipped in
`cmsis-device-f1-master/Include`, which is why that directory is gone rather
than the root copies. To re-add a HAL peripheral, copy its `.c`/`.h` back from
a fresh `stm32f1xx-hal-driver` checkout, add the `.c` to `HAL_SRCS` and enable
its `HAL_*_MODULE_ENABLED` in `inc/stm32f1xx_hal_conf.h`.

## Demo application

Pins on a Blue Pill:

- **PB0 → GND** (hold): pointer walks right, then back left
- **PB1 → GND** (tap): types `Hello from STM32 HID!`
- **PC13**: on-board LED lit once the host has configured the device

Both buttons use the internal pull-up, so with nothing wired the firmware sends
no reports and leaves the mouse and keyboard alone.

Sending your own reports:

```c
HID_Mouse_Send(10, 0, 0, 0);                   /* 10 steps right */
HID_Mouse_Click(HID_MOUSE_BTN_LEFT);           /* press + release */
HID_Keyboard_Tap(HID_MOD_LGUI, HID_KEY_D);     /* Win+D */
HID_Keyboard_TypeString("hello\n");            /* US layout ASCII */
```

`HID_Ready()` returns 1 only when the host has configured the device and the
endpoint is free, so poll it before pushing reports from a tight loop.

### Keyboard layout caveat

`HID_Keyboard_TypeString()` sends usage IDs, not characters, and the host
applies whatever keyboard layout is active. Verified on this machine: with the
Thai layout selected, `Hello from STM32 HID!` arrived as correct usage IDs but
showed up as `สสน ดพนท ฆธ?-/ ็ณฏ+`; with the English layout it typed
`Hello from STM32 HID!` byte for byte. Switch the input language to English
before testing typed output, or map characters for the target layout instead of
the US table in `HID_AsciiToKey()`.

## ⚠️ Before trusting a build: check `linker.ld`

`unitymbed init` rewrites `linker.ld` from the IDE template, and that template
keeps the wrong section name for the vector table:

```ld
.text : { KEEP(*(.vectors)) ... }      /* template - WRONG for STM32 */
```

ST's CMSIS gcc startup emits the vector table into `.isr_vector`, not
`.vectors`. With `-Wl,--gc-sections` the linker therefore discards the whole
table: the build succeeds with no warning, flashing succeeds and verifies, and
the MCU then loads a garbage stack pointer at reset and locks up before
`main()`. On a Blue Pill the hardware D+ pull-up still makes the host see a
device, so the symptom looks like a USB bug ("Unknown USB Device (Device
Descriptor Request Failed)", code 43) rather than a dead chip.

The fix already applied here keeps both names:

```ld
KEEP(*(.isr_vector))
KEEP(*(.vectors))
```

**Re-run this check after every `unitymbed init`, and whenever a build boots
into nothing:**

```sh
grep -q isr_vector linker.ld && echo "linker OK" || echo "linker BROKEN - vector table will be stripped"
arm-none-eabi-nm build/*.elf | grep -qiE "g_pfnVectors|vector_table" && echo "vector table present" || echo "vector table MISSING"
od -A x -t x4 -N 4 build/*.bin   # word 0 must be a RAM address (0x2000xxxx), not code
```

## Checking the device from the host

```powershell
../../tools/hid-sniffer.ps1 -Match 'VID_0483&PID_5750'                    # how Windows parsed the report descriptor
../../tools/hid-sniffer.ps1 -Match 'VID_0483&PID_5750' -Live -Seconds 12  # live reports, via Raw Input
```

Expected descriptor output: a keyboard collection (usage page 0x01, usage 0x06,
9-byte input report) and a mouse collection (usage 0x02, 5-byte input report,
3 value caps for X/Y/wheel).
