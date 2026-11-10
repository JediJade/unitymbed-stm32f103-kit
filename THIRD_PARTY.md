# Third-party files

Everything not listed here was written for this kit and is under the MIT
licence in `LICENSE`.

| What | Where | Owner | Licence |
| --- | --- | --- | --- |
| CMSIS core headers: `core_cm3.h`, `cmsis_gcc.h`, `cmsis_compiler.h`, `cmsis_version.h` | `common/cmsis/`, `templates/*/cmsis/`, root of `templates/usb-*-hal/` | Arm Limited | Apache-2.0 (header of each file) |
| STM32F1 device headers: `stm32f103xb.h`, `stm32f1xx.h`, `system_stm32f1xx.h` | same places | STMicroelectronics | BSD-3-Clause (header of each file) |
| `cmsis-device-f1` - `system_stm32f1xx.c`, `startup_stm32f103xb.s` | `templates/usb-*-hal/cmsis-device-f1-master/` | STMicroelectronics | Apache-2.0, `License.md` alongside |
| STM32F1 HAL driver (subset) | `templates/usb-*-hal/stm32f1xx-hal-driver-master/` | STMicroelectronics | BSD-3-Clause, `LICENSE.md` alongside |
| STM32 USB device library (Core, and the CDC class) | `templates/usb-*-hal/stm32-mw-usb-device-master/` | STMicroelectronics | SLA0044 (Ultimate Liberty), `LICENSE.md` alongside. Permits redistribution in source form, for use on ST microcontrollers only. |

Upstream sources:

- https://github.com/ARM-software/CMSIS_5
- https://github.com/STMicroelectronics/cmsis-device-f1
- https://github.com/STMicroelectronics/stm32f1xx-hal-driver
- https://github.com/STMicroelectronics/stm32-mw-usb-device

**Not included:** the ST-Link USB driver (`STSW-LINK009`). ST distributes it
under its own licence from st.com, so this repository links to it instead of
copying it - see `drivers/windows/README.md`.

The bare-metal templates (`blink-bare`, `usb-hid-bare`, `usb-cdc-bare`) use
only the headers in the first two rows: no HAL, no ST USB library.
