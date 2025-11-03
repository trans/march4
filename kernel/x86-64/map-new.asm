; march.map.new ( -- map )
; Creates a new empty persistent hash map
; Returns NULL (lazy allocation on first insert)

section .text
extern hamt_new
extern vm_dispatch
global op_map_new

op_map_new:
    ; rsi = data stack pointer
    ; rdi = return stack pointer
    ; rbx = instruction pointer

    ; Save caller-saved VM registers
    push rdi
    push rsi

    ; Align stack to 16 bytes for C call
    mov rbp, rsp
    and rsp, -16

    ; Call hamt_new() - no arguments
%ifdef PIC
    call hamt_new wrt ..plt
%else
    call hamt_new
%endif

    ; Restore stack
    mov rsp, rbp

    ; Restore VM registers
    pop rsi
    pop rdi

    ; Push map pointer to data stack (will be NULL)
    sub rsi, 8
    mov [rsi], rax

    jmp vm_dispatch
