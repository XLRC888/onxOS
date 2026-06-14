MBALIGN  equ  1 << 0
MEMINFO  equ  1 << 1
VIDMODE  equ  1 << 2
FLAGS    equ  MBALIGN | MEMINFO | VIDMODE
MAGIC    equ  0x1BADB002
CHECKSUM equ -(MAGIC + FLAGS)
section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM
    dd 0
    dd 0
    dd 0
    dd 0
    dd 0
    dd 1
    dd 80
    dd 25
    dd 0
section .bss
align 16
stack_bottom:
resb 16384
stack_top:
section .text
global _start:function (_start.end - _start)
_start:
    mov esp, stack_top
    mov dword [0xB8000], 0x0F4B0F4F
    mov dword [0xB8004], 0x0F20
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
