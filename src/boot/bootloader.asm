ORG 0x7C00
BITS 16

; stage1: MBR loaded by BIOS at 0x7C00
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    mov [boot_drive], dl

    ; load stage2 (sectors 1-5) to 0x7E00 via extended read
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
; partition table (filled by setup)
times 64 db 0
dw 0xAA55

; stage2: loaded at 0x7E00, entered with DL = boot drive
stage2:
    ; patched entry point (first 4 bytes of stage2)
    kern_entry: dd 0x200020

    ; save boot drive
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
    ; enable A20 via fast A20
    in al, 0x92
    or al, 2
    out 0x92, al

    ; load GDT
    lgdt [gdtr]

    ; switch to protected mode
    cli
    mov eax, cr0
    or al, 1
    mov cr0, eax

    ; far jump to reload CS
    jmp 0x08:pm_entry

BITS 32
pm_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x10000

    ; copy kernel from 0x10000 to 0x200000 (122 sectors = 62464 bytes)
    mov esi, 0x10000
    mov edi, 0x200000
    mov ecx, 62464
    cld
    rep movsb

    ; jump to kernel entry point
    mov eax, [kern_entry]
    jmp eax

BITS 16
st2_drive: db 0
dap_kern:
    db 0x10, 0
    dw 122
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
