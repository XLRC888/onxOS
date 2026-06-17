section .multiboot
align 8
mb_start:
    dd 0xe85250d6
    dd 0
    dd mb_end - mb_start
    dd 0x100000000 - (0xe85250d6 + 0 + (mb_end - mb_start))
    dw 0
    dw 0
    dd 8
mb_end:
section .bss
align 16
stack_bottom:
    resb 16384
stack_top:
section .text
global _start:function (_start.end - _start)
_start:
    mov esp, stack_top
    mov edx, ebx
    mov ebp, eax
    extern start_of_bss
    extern end_of_kernel
    mov edi, start_of_bss
    mov ecx, end_of_kernel
    sub ecx, edi
    xor eax, eax
    cld
    rep stosb
    push edx
    push ebp
    cli
    extern kernel_early
    call kernel_early
    extern kernel_main
    call kernel_main
    cli
.hang:
    hlt
    jmp .hang
.end:
