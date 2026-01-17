/* cool-os kernel linker script for Limine bootloader */

OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)

PHDRS
{
    requests PT_LOAD FLAGS(6); /* rw - Limine requests */
    text     PT_LOAD FLAGS(5); /* rx */
    rodata   PT_LOAD FLAGS(4); /* r  */
    data     PT_LOAD FLAGS(6); /* rw */
}

SECTIONS
{
    . = 0xFFFFFFFF80000000;

    .limine_requests : {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    } :requests

    . = ALIGN(4096);
    .text : {
        *(.text .text.*)
    } :text

    . = ALIGN(4096);
    .rodata : {
        *(.rodata .rodata.*)
    } :rodata

    . = ALIGN(4096);
    .data : {
        *(.data .data.*)
    } :data

    . = ALIGN(4096);
    .bss : {
        *(COMMON)
        *(.bss .bss.*)
    } :data

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
