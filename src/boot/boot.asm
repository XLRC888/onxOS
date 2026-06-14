MBALIGN  equ  1 << 0
MEMINFO  equ  1 << 1
FLAGS    equ  MBALIGN | MEMINFO
MAGIC    equ  0x1BADB002
CHECKSUM equ -(MAGIC + FLAGS)
section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM
section .bss
align 16
stack_bottom:
resb 16384
stack_top:
section .text
global _start:function (_start.end - _start)
_start:
    mov dword [0xB8000], 0x4F4B
    mov dword [0xB8004], 0x0F20
    mov esp, stack_top
    cli
    push ebx
    push eax
    extern kernel_early
    call kernel_early
    extern kernel_main
    call kernel_main
    cli
.hang:
    hlt
    jmp .hang
.end:
