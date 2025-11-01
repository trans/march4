; str-length ( str -- length )
; Read count field from string header (offset 0)

section .text
extern vm_dispatch
global op_str_length

op_str_length:
    ; rsi = data stack pointer
    ; [rsi] = string pointer

    mov rax, [rsi]          ; rax = string pointer
    mov rax, [rax]          ; rax = count (first 8 bytes of string)
    mov [rsi], rax          ; Replace string pointer with count

    jmp vm_dispatch
