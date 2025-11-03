; march.map.size ( map -- count )
; Returns number of key-value pairs in map
; O(1) operation - size is cached in root header

section .text
extern hamt_size
extern vm_dispatch
global op_map_size

op_map_size:
    ; rsi = data stack pointer
    ; Stack layout: [map] <- TOS

    ; Get argument from data stack
    mov r10, [rsi]          ; map (TOS)
    add rsi, 8              ; Pop map argument

    ; Save caller-saved VM registers
    push rdi
    push rsi

    ; Set up C function argument
    ; uint64_t hamt_size(void* node)
    mov rdi, r10            ; First arg = map pointer

    ; Align stack to 16 bytes for C call
    mov rbp, rsp
    and rsp, -16

    ; Call hamt_size(map)
%ifdef PIC
    call hamt_size wrt ..plt
%else
    call hamt_size
%endif

    ; Restore stack
    mov rsp, rbp

    ; Restore VM registers
    pop rsi
    pop rdi

    ; Push count result to data stack
    sub rsi, 8
    mov [rsi], rax          ; Store size

    jmp vm_dispatch
