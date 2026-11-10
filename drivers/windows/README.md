# Windows drivers - what is needed, and what is not

Short version: **only the ST-Link needs a driver.** The USB devices these
templates create use drivers Windows 10/11 already has.

| Device | VID:PID | Driver | Install anything? |
| --- | --- | --- | --- |
| ST-Link V2 (programmer) | 0483:3748 | ST `STSW-LINK009` (WinUSB) | **Yes, once** |
| ST-Link V2-1 / V3 (Nucleo, Discovery) | 0483:374B, 374E, 374F, ... | ST `STSW-LINK009` | **Yes, once** |
| `usb-cdc-bare` | 0483:5752 | `usbser.sys` (built in) | No |
| `usb-cdc-hal` | 0483:5740 | `usbser.sys` (built in) | No |
| `usb-hid-bare` | 0483:5751 | `hidusb.sys` / `kbdhid` / `mouhid` (built in) | No |
| `usb-hid-hal` | 0483:5750 | built-in HID drivers | No |
| USB-TTL adapter, CH340 | 1A86:7523 | WCH CH341SER | Usually Windows Update finds it |
| USB-TTL adapter, CP2102 | 10C4:EA60 | Silicon Labs CP210x VCP | Usually Windows Update finds it |

## ST-Link: STSW-LINK009

This repository does **not** include the driver - ST distributes it under its
own licence. Download it from ST:

<https://www.st.com/en/development-tools/stsw-link009.html>

(ST asks you to log in or give an e-mail address before the download starts.)

Install:

1. Unplug every ST-Link.
2. Unzip, right-click `stlink_winusb_install.bat` → **Run as administrator**.
   If that fails, run `dpinst_amd64.exe` (64-bit Windows) as administrator.
3. Plug the ST-Link in. Device Manager should show it under
   *Universal Serial Bus devices* as **STM32 STLink**, with no warning sign.

Check from PowerShell:

```powershell
Get-PnpDevice -PresentOnly | Where-Object InstanceId -match 'VID_0483' |
    Format-Table Status, Class, FriendlyName -AutoSize
```

or watch devices come and go with `tools/usb-sniffer.ps1`.

A plain ST-Link V2 dongle has **no** virtual COM port. For UART output, wire
the board's TX/RX to a USB-TTL adapter, or use one of the USB CDC templates
and the Blue Pill's own USB socket.

## Wiring an ST-Link V2 to a Blue Pill

| ST-Link V2 | Blue Pill SWD header |
| --- | --- |
| SWDIO | DIO (PA13) |
| SWCLK | CLK (PA14) |
| GND | GND |
| 3.3V | 3.3 (only if the board is not powered from its USB socket) |

Leave BOOT0 at 0.

## When something does not show up

| Symptom | Likely cause |
| --- | --- |
| `Error: open failed` from OpenOCD | ST-Link driver not installed, or another program (STM32CubeProgrammer, another OpenOCD) holds the ST-Link |
| ST-Link found, `Error connecting DP` | Wiring, no power on the target, or the firmware turned PA13/PA14 into GPIO. Hold the board's RESET button, start flashing, release it |
| Blue Pill shows "Unknown USB Device (Device Descriptor Request Failed)", code 43 | First check the vector table (`sh tools/build-all.sh`): a stripped table looks exactly like this. Then check the 72 MHz / 48 MHz clock setup and the cable |
| Works once, then not after reflashing | Windows kept the old session. The templates pulse D+ low at start-up to force re-enumeration; replugging does the same |
| Device shows up with a stale name or wrong interfaces after changing descriptors | Windows caches descriptors per VID:PID. Change the PID (each template uses its own for this reason) or uninstall the device in Device Manager |
| COM port opens but nothing arrives | Another terminal already holds the port: only one program can open it at a time |
