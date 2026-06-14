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
