; swap ( a b -- b a )
; Exchanges the top two stack items
; Stack effect: Swap TOS and second item

section .text
global op_swap

op_swap:
    ; rsi = data stack pointer
    ; [rsi] = TOS (b)
    ; [rsi+8] = second (a)

    mov rax, [rsi]          ; Load b
    mov rbx, [rsi + 8]      ; Load a
    mov [rsi], rbx          ; Store a at TOS
    mov [rsi + 8], rax      ; Store b at second
    ret
