STAGE2_SECT equ 5
%ifndef KERN_SECT
KERN_SECT equ 72
%endif
DATA_LBA equ STAGE2_SECT + 1 + KERN_SECT
KERN_TMP equ 0x10000
KERN_DST equ 0x100000
KERN_ENTRY equ 0x100010
%ifndef BSS_SZ
BSS_SZ equ 0x14000
%endif
%ifndef KERN_FSZ
KERN_FSZ equ 0x8f40
%endif
section .text
bits 16
org 0x7C00
stage1:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    mov [drive], dl
    cld
    mov si, dap
    mov ah, 0x42
    int 0x13
    jc .err
    mov si, dap2
    mov ah, 0x42
    int 0x13
    jc .err
    jmp 0x0000:0x7E00
.err:
    mov si, .msg
.l: lodsb
    or al, al
    jz .h
    mov ah, 0x0E
    int 0x10
    jmp .l
.h: hlt
    jmp .h
.msg db "onxOS boot fail", 0
align 4
dap:
    db 0x10, 0
    dw STAGE2_SECT
    dw 0x7E00, 0
    dq 1
dap2:
    db 0x10, 0
    dw KERN_SECT
    dw 0x0000, 0x1000
    dq STAGE2_SECT + 1
drive db 0
times 446 - ($ - $$) db 0
pt1:
    db 0x80
    db 0, 2, 0
    db 0xDA
    db 0, 0, 0
    dd DATA_LBA
    dd 0
pt2: times 16 db 0
pt3: times 16 db 0
pt4: times 16 db 0
dw 0xAA55
stage2_start:
bits 16
    xor ax, ax
    mov ds, ax
    mov es, ax
    cli
    in al, 0x92
    or al, 2
    out 0x92, al
    lgdt [gdtr]
    mov eax, cr0
    or al, 1
    mov cr0, eax
    jmp 0x08:.pm
bits 32
.pm:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x1000
    cld
    mov esi, KERN_TMP
    mov edi, KERN_DST
    mov ecx, KERN_FSZ
    rep movsb
    mov edi, KERN_DST + KERN_FSZ
    mov ecx, BSS_SZ - KERN_FSZ
    xor al, al
    rep stosb
    mov dword [mbi_loc], 0
    mov eax, 0x2BADB002
    mov ebx, mbi_loc
    jmp KERN_ENTRY
align 8
gdt:
    dq 0
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
gdtr:
    dw 23
    dd gdt
align 4
mbi_loc:
    dd 0
times (STAGE2_SECT * 512) - ($ - stage2_start) db 0
