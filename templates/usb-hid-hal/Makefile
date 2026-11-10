CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

PROJECT ?= firmware
TARGET   = $(PROJECT)
OUTDIR   = build
CPU = cortex-m3

HAL_SRCS = \
    stm32f1xx-hal-driver-master/Src/stm32f1xx_hal.c \
    stm32f1xx-hal-driver-master/Src/stm32f1xx_hal_cortex.c \
    stm32f1xx-hal-driver-master/Src/stm32f1xx_hal_rcc.c \
    stm32f1xx-hal-driver-master/Src/stm32f1xx_hal_rcc_ex.c \
    stm32f1xx-hal-driver-master/Src/stm32f1xx_hal_gpio.c \
    stm32f1xx-hal-driver-master/Src/stm32f1xx_hal_pcd.c \
    stm32f1xx-hal-driver-master/Src/stm32f1xx_hal_pcd_ex.c \
    stm32f1xx-hal-driver-master/Src/stm32f1xx_ll_usb.c \
    stm32f1xx-hal-driver-master/Src/stm32f1xx_hal_flash.c \
    stm32f1xx-hal-driver-master/Src/stm32f1xx_hal_flash_ex.c

# USB device core only - the HID class driver is src/usbd_hid_if.c, so no
# class from stm32-mw-usb-device-master/Class is compiled.
USB_SRCS = \
    stm32-mw-usb-device-master/Core/Src/usbd_core.c \
    stm32-mw-usb-device-master/Core/Src/usbd_ctlreq.c \
    stm32-mw-usb-device-master/Core/Src/usbd_ioreq.c

CMSIS_SRCS = \
    cmsis-device-f1-master/Source/Templates/system_stm32f1xx.c \
    cmsis-device-f1-master/Source/Templates/gcc/startup_stm32f103xb.s

SRCS   := $(wildcard src/*.c) $(HAL_SRCS) $(USB_SRCS) $(CMSIS_SRCS)

CFLAGS  = -mcpu=$(CPU) -mthumb -Os -g -ffreestanding -Wall -I. -Iinc -Istm32f1xx-hal-driver-master/Inc -Istm32-mw-usb-device-master/Core/Inc -DSTM32F103xB $(CFLAGS_EXTRA)
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
