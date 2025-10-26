; i0 - Get current loop index
; Reads from return stack TOS without popping
; Stack effect: ( -- i64 )

section .text
    global op_i0
    extern vm_dispatch

op_i0:
    ; rsi = data stack pointer
    ; rdi = return stack pointer (TOS)
    ; rbx = IP

    ; Load loop counter from return stack (without popping)
    mov rax, [rdi]          ; Load TOS of return stack

    ; Push to data stack
    sub rsi, 8
    mov [rsi], rax

    jmp vm_dispatch
