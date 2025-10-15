; ! ( value addr -- )
; Stores a 64-bit value to memory address
; Stack effect: Pop value and address, write value to address

section .text
global op_store

op_store:
    ; rsi = data stack pointer
    ; [rsi] = TOS (addr)
    ; [rsi+8] = second (value)

    mov rax, [rsi]          ; Load address
    mov rbx, [rsi + 8]      ; Load value
    mov [rax], rbx          ; Store value at address
    add rsi, 16             ; Drop both items
    ret
