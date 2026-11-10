/* Sizes are rewritten by `unitymbed init` from the detected part.
   Defaults are the smallest common STM32 footprint, so an undetected
   build still links and runs on anything. */
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 64K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}
ENTRY(Reset_Handler)

/* Top of stack: first word of the vector table. Kept outside SECTIONS so it
   is a true absolute symbol. */
_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
  .text :
  {
    /* The vector table MUST be the first thing in FLASH, and must survive
       --gc-sections. ST's CMSIS gcc startup emits it as .isr_vector;
       other templates use .vectors - keep both. */
    KEEP(*(.isr_vector))
    KEEP(*(.vectors))
    *(.text*)
    *(.rodata*)
    . = ALIGN(4);
    _sidata = .;
  } > FLASH

  .data : AT(_sidata)
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } > RAM

  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } > RAM

  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) }
}
