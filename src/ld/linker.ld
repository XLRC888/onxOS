ENTRY(_start)

SECTIONS
{
    . = 1M;

    .multiboot : {
        KEEP(*(.multiboot))
    }

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
        . = ALIGN(4096);
        end_of_kernel = .;
    }
}
