CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

# UnityMbed passes PROJECT=<project name>; a plain `make` builds build/firmware.*
PROJECT ?= firmware
TARGET   = $(PROJECT)
OUTDIR   = build
CPU      = cortex-m3

# Every source there is: no HAL, no USB middleware, no CMSIS .c files.
#   src/startup.c  vector table + Reset_Handler
#   src/main.c     clocks, GPIO, echo loop
#   src/usb_cdc.c  USB CDC-ACM device on the registers
SRCS := $(wildcard src/*.c)

CFLAGS  = -mcpu=$(CPU) -mthumb -Os -g -ffreestanding -Wall -Wextra \
          -Iinc -Icmsis -DSTM32F103xB $(CFLAGS_EXTRA)
LDFLAGS = -nostdlib -Wl,-T,linker.ld -Wl,--gc-sections -Wl,-Map=$(OUTDIR)/$(TARGET).map

all: $(OUTDIR)/$(TARGET).elf $(OUTDIR)/$(TARGET).bin $(OUTDIR)/$(TARGET).hex

$(OUTDIR)/$(TARGET).elf: $(SRCS) linker.ld
	@mkdir -p $(OUTDIR)
	$(CC) $(CFLAGS) $(SRCS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(OUTDIR)/$(TARGET).bin: $(OUTDIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(OUTDIR)/$(TARGET).hex: $(OUTDIR)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

flash: $(OUTDIR)/$(TARGET).hex
	openocd -f openocd.cfg -c "program $< verify reset exit"

clean:
	rm -rf $(OUTDIR)

.PHONY: all clean flash
