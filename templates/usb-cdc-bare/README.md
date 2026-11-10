# STM32F103 USB CDC (virtual COM port) - bare metal

A USB virtual COM port written straight on the STM32F103 registers: no HAL,
no ST USB device library. The demo echoes every byte the PC sends. About
2.7 KB of flash.

| file | role |
| --- | --- |
| `src/usb_cdc.c` | CDC-ACM device: PMA, BTABLE, EP0 control transfers incl. the OUT data stage, bulk IN/OUT |
| `inc/usb_cdc.h` | `usb_cdc_init()`, `usb_cdc_configured()`, `usb_cdc_write()`, `usb_cdc_read_byte()` |
| `src/main.c` | 72 MHz clock, SysTick, D+ re-enumeration pulse, echo loop |
| `src/startup.c` | vector table (`USB_LP_CAN1_RX0` and `USBWakeUp` wired to the driver) |
| `cmsis/` | register definitions only |

## Interface

```
VID 0x0483  PID 0x5752  "STM32 Bare Metal CDC"
interface 0: CDC communication (ACM), notification EP 0x82, interrupt, 16 B
interface 1: CDC data, EP 0x01 bulk OUT 64 B, EP 0x81 bulk IN 64 B
```

Windows 10/11 binds it to the built-in `usbser.sys` - no driver to install.
Baud rate, parity and stop bits are accepted and ignored: there is no UART
behind this port.

## Use it

```sh
make PROJECT=usb-cdc-bare
make flash PROJECT=usb-cdc-bare
```

Plug the Blue Pill's own USB socket into the PC (the ST-Link only programs it),
then open the new COM port in any terminal, or:

```powershell
..\..\tools\serial-sniffer.ps1 -List     # find the port
```

The PC13 LED lights once the PC has configured the device. Type, and the
characters come back; Enter prints a new `>` prompt.

The API in your own code:

```c
usb_cdc_init();
if (usb_cdc_configured() && usb_cdc_write_ready()) {
  usb_cdc_write((const uint8_t *)"hi\r\n", 4);   /* at most 64 bytes per call */
}
uint8_t c;
while (usb_cdc_read_byte(&c)) { /* ... */ }
```

`usb_cdc_write()` returns 0 while the previous packet is still in flight, so
use the `cdc_send()` helper in `main.c` for longer buffers - it waits for the
endpoint and gives up after 100 ms if nothing on the PC is reading.

## Status

`src/usb_cdc.c` is the driver from the STM32FOC project, where it ran on
hardware as a 12-command console (COM13, 17 of 17 host-side command checks
passing). Here only its product and serial strings were changed. `main.c` is
new. It builds with no warnings, but **this exact image has not been flashed
yet.**
