ENTRY(_start)

PHDRS
{
    all PT_LOAD FLAGS(7);
}

SECTIONS
{
    . = 1M;

    .multiboot : {
        KEEP(*(.multiboot))
    } :all

    .text : {
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(COMMON)
        *(.bss)
        *(.bss*)
        . = ALIGN(4096);
        end_of_kernel = .;
    }
}
