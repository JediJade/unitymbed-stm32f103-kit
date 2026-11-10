# common/ - shared pieces, for projects that do not start from a template

Every template already carries its own copy of these files, so a template
folder builds on its own after being copied anywhere. This folder is the
reference set, for adding STM32F103 support to an existing project.

| file | what it is |
| --- | --- |
| `cmsis/stm32f103xb.h` | STM32F103 register map (ST, BSD-3-Clause) - every peripheral as a struct, every bit as a `#define` |
| `cmsis/system_stm32f1xx.h` | `SystemInit()` / `SystemCoreClock` declarations (ST) |
| `cmsis/core_cm3.h`, `cmsis_gcc.h`, `cmsis_compiler.h`, `cmsis_version.h` | Cortex-M3 core: NVIC, SysTick, `__enable_irq()` ... (Arm, Apache-2.0) |
| `linker/stm32f103x8.ld` | 64 KB flash / 20 KB RAM. The vector table goes in its own `.isr_vector` section, first in flash, `KEEP`ed so `--gc-sections` cannot drop it |
| `openocd/stlink-stm32f1.cfg` | ST-Link over SWD, `stm32f1x` flash bank at 0x08000000 |

Compile with `-mcpu=cortex-m3 -mthumb -DSTM32F103xB -Icmsis`.

Many "STM32F103C8" Blue Pills really have 128 KB of flash. To use it, change
`LENGTH = 64K` to `128K` in the linker script - OpenOCD reads the real size
from the chip, so flashing past 64 KB works only if the silicon has it.

Flash with the OpenOCD that UnityMbed installs (the xPack build):

```sh
"$LOCALAPPDATA/UnityMbed/tools/openocd-xpack/bin/openocd.exe" \
  -f common/openocd/stlink-stm32f1.cfg -c "program build/app.hex verify reset exit"
```

The config creates the debug port directly instead of using
`target/stm32f1x.cfg`, so it does not check the chip's ID code and clone parts
(CKS32, CS32, APM32, GD32) are not refused at connect time. Flashing has only
been checked on the boards used to build this kit, not on every clone.
