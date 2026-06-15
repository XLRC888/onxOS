ENTRY(_start)

SECTIONS
{
    . = 2M;

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
        start_of_bss = .;
        *(COMMON)
        *(.bss)
        *(.bss*)
        . = ALIGN(4096);
        end_of_kernel = .;
    }

    /DISCARD/ : {
        *(.note.GNU-stack)
        *(.note*)
        *(.comment*)
    }
}
