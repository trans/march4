; array-length ( array -- length )
; Read count field from array header (offset 0)

section .text
extern vm_dispatch
global op_array_length

op_array_length:
    ; rsi = data stack pointer
    ; [rsi] = array pointer

    mov rax, [rsi]          ; rax = array pointer
    mov rax, [rax]          ; rax = count (first 8 bytes of array)
    mov [rsi], rax          ; Replace array pointer with count

    jmp vm_dispatch
