global ll_setjmp
global ll_longjmp
section .text
ll_setjmp:
    mov ecx, [esp+4]
    mov [ecx], ebx
    mov [ecx+4], esi
    mov [ecx+8], edi
    mov [ecx+12], ebp
    mov [ecx+16], esp
    mov eax, [esp]
    mov [ecx+20], eax
    xor eax, eax
    ret
ll_longjmp:
    mov ecx, [esp+4]
    mov eax, [esp+8]
    mov ebx, [ecx]
    mov esi, [ecx+4]
    mov edi, [ecx+8]
    mov ebp, [ecx+12]
    mov esp, [ecx+16]
    jmp [ecx+20]
