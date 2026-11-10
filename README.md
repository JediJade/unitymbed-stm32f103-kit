# UnityMbed STM32F103 kit

Ready-to-build starting points for the STM32F103 ("Blue Pill") with an
ST-Link, laid out the way the UnityMbed IDE expects a
project: `Makefile`, `linker.ld`, `openocd.cfg`, `unitymbed.json`. Every
template also builds as a plain `make` project in Git Bash or Linux.

ภาษาไทย: [README.th.md](README.th.md)

## Templates

| template | what it does | flash | built on | status |
| --- | --- | --- | --- | --- |
| [`blink-bare`](templates/blink-bare) | PC13 LED blink, 72 MHz clock, SysTick | 0.5 KB | registers | builds; not yet flashed |
| [`usb-cdc-bare`](templates/usb-cdc-bare) | USB virtual COM port, echo | 2.7 KB | registers | driver ran on hardware in another project; this image not yet flashed |
| [`usb-hid-bare`](templates/usb-hid-bare) | USB keyboard + mouse | 3.2 KB | registers | ran on hardware before the 2026-11-10 `usb.c` fix; current image not yet flashed |
| [`usb-cdc-hal`](templates/usb-cdc-hal) | USB virtual COM port, streams ADC on PA0 | 22 KB | ST HAL + USB library | **verified on hardware** |
| [`usb-hid-hal`](templates/usb-hid-hal) | USB keyboard + mouse | 16 KB | ST HAL + USB library | **verified on hardware** |

"Verified on hardware" means the template builds to a `.bin` that is
byte-identical to a project that was run on a Blue Pill (compared by sha256
with the arm-none-eabi-gcc 14.2.1 that UnityMbed ships). `usb-hid-bare` had
that status until its USB driver was fixed (a lost-`CTR` race, also fixed in
`usb-cdc-bare`); its image now differs from the one that was run - see its
[README](templates/usb-hid-bare/README.md#status).

Bare-metal templates use only the CMSIS and STM32F103 headers. The HAL
templates carry the subset of ST's HAL, CMSIS-device and USB device library
they compile, not the whole vendor trees.

## Repository layout

```
templates/        one folder per starting point - copy one and build
common/           CMSIS headers, linker script, OpenOCD config (reference set)
drivers/windows/  which drivers are needed (only the ST-Link's), wiring, troubleshooting
tools/
  new-project.ps1   copy a template into ~/UnityMbed/<Name> and rename it
  build-all.sh      build every template and check each vector table
  serial-sniffer.ps1  read a COM port (CDC templates, ST-Link VCP, USB-TTL)
  hid-sniffer.ps1     how Windows parsed a HID device, and its live reports
  usb-sniffer.ps1     watch ST / STM32 USB devices arrive and leave
```

## Start a project

1. Install the UnityMbed IDE. It brings arm-none-eabi-gcc,
   make and OpenOCD.
2. Install the ST-Link driver, once: see [drivers/windows](drivers/windows/README.md).
3. Make a project from a template:

   ```powershell
   .\tools\new-project.ps1 -List
   .\tools\new-project.ps1 -Template usb-cdc-bare -Name MySensor
   ```

   That creates `%USERPROFILE%\UnityMbed\MySensor` and sets `"name"` in its
   `unitymbed.json`. Copying a template folder by hand works too.
4. Open the folder in UnityMbed, build, flash. Or from Git Bash:

   ```sh
   make PROJECT=MySensor
   make flash PROJECT=MySensor
   ```

## Wiring

| ST-Link V2 | Blue Pill |
| --- | --- |
| SWDIO | DIO (PA13) |
| SWCLK | CLK (PA14) |
| GND | GND |
| 3.3V | 3.3 |

BOOT0 = 0. For the USB templates, also plug the Blue Pill's own USB socket
into the PC: the ST-Link only programs the chip.

## Two traps worth knowing first

**`unitymbed init` can break the vector table.** It rewrites `linker.ld` from
the IDE's template, which has used the section name `.vectors`; these
projects put the table in `.isr_vector`. With `--gc-sections` the table is
then silently discarded: the build succeeds, flashing verifies, and the chip
locks up before `main()`. A Blue Pill still appears on USB because of its D+
pull-up, so it looks like a USB problem ("Unknown USB Device", code 43). The
linker scripts here are safe; after any `unitymbed init`, run:

```sh
sh tools/build-all.sh
```

It fails any template whose image does not start with a RAM stack pointer and
an odd (Thumb) reset vector.

**Windows caches USB descriptors per VID:PID.** Each template has its own PID
(0x5740, 0x5750, 0x5751, 0x5752) so one firmware never inherits another's
cached descriptors. Give your own projects their own PID too. 0x0483 is ST's
vendor ID and is fine for development on your own desk, but a product you
ship needs a VID/PID of its own.

## Licence

The code written for this kit is MIT ([LICENSE](LICENSE)). Files from Arm and
STMicroelectronics keep their own licences, listed in
[THIRD_PARTY.md](THIRD_PARTY.md). The ST-Link driver is not included; it is
downloaded from st.com.
