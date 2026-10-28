OUTPUT_ARCH(riscv)
ENTRY(_start)
MEMORY {
    ROM (rx)  : ORIGIN = 0x00000000, LENGTH = 4M
    RAM (rwx) : ORIGIN = 0x10000000, LENGTH = 4M
}
SECTIONS {
    .text : {
        KEEP(*(.text.start))
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
    } > ROM
    .data : {
        _data_start = .;
        *(.data .data.* .sdata .sdata.*)
        . = ALIGN(4);
        _data_end = .;
    } > RAM AT> ROM
    _data_load = LOADADDR(.data);
    .bss (NOLOAD) : {
        _bss_start = .;
        *(.bss .bss.* .sbss .sbss.* COMMON)
        . = ALIGN(4);
        _bss_end = .;
    } > RAM
    _stack_top = ORIGIN(RAM) + LENGTH(RAM);
    ASSERT(_bss_end + 4096 <= _stack_top, "RAM space insufficient for stack")
    /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.note*) }
}
