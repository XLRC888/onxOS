ORG 0x7C00
BITS 16

    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    mov [boot_drive], dl

    mov si, dap_stage2
    mov ah, 0x42
    int 0x13
    jnc .go_stage2
    xor ax, ax
    int 0x13
    mov si, dap_stage2
    mov ah, 0x42
    int 0x13
    jnc .go_stage2
.err:
    cli
    hlt
    jmp .err
.go_stage2:
    mov dl, [boot_drive]
    jmp 0x0000:0x7E00

boot_drive: db 0
dap_stage2:
    db 0x10, 0
    dw 5
    dw 0x7E00
    dw 0
    dd 1
    dd 0

times 446-($-$$) db 0
times 64 db 0
dw 0xAA55

stage2:
    jmp real_start
    kern_entry: dd 0x200020

real_start:
    mov [st2_drive], dl

    mov si, dap_kern
    mov ah, 0x42
    int 0x13
    jc .kerr
    jmp .kdone
.kerr:
    xor ax, ax
    int 0x13
    mov si, dap_kern
    mov ah, 0x42
    int 0x13
    jnc .kdone
    cli
    hlt
    jmp .kerr
.kdone:
    in al, 0x92
    or al, 2
    out 0x92, al

    push es
    xor ax, ax
    mov es, ax
    mov ax, [es:0x7DFE]
    not ax
    mov [es:0x7E00], ax
    push 0xFFFF
    pop ds
    cmp ax, [0x7E10]
    pop es
    jne a20_ok
    mov al, 0xD1
    out 0x64, al
a20_wait:
    in al, 0x64
    test al, 2
    jnz a20_wait
    mov al, 0xDF
    out 0x60, al
a20_ok:
    xor ax, ax
    mov ds, ax

    lgdt [gdtr]

    cli
    mov eax, cr0
    or al, 1
    mov cr0, eax

    jmp 0x08:pm_entry

BITS 32
pm_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x10000

    mov esi, 0x10000
    mov edi, 0x200000
    mov ecx, 124928
    cld
    rep movsb

    mov eax, [kern_entry]
    jmp eax

BITS 16
st2_drive: db 0
dap_kern:
    db 0x10, 0
    dw 244
    dw 0x0000
    dw 0x1000
    dd 6
    dd 0

gdt:
    dq 0
    db 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x9A, 0xCF, 0x00
    db 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x92, 0xCF, 0x00
gdt_end:
gdtr:
    dw gdt_end - gdt - 1
    dd gdt

times 2560-($-stage2) db 0
