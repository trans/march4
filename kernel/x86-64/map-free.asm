; march.map.free ( map -- )
; Recursively frees all nodes in persistent hash map
; WARNING: Does not free values if they are heap pointers!

section .text
extern hamt_free
extern vm_dispatch
global op_map_free

op_map_free:
    ; rsi = data stack pointer
    ; Stack layout: [map] <- TOS

    ; Get argument from data stack
    mov r10, [rsi]          ; map (TOS)
    add rsi, 8              ; Pop map argument

    ; Save caller-saved VM registers
    push rdi
    push rsi

    ; Set up C function argument
    ; void hamt_free(void* node)
    mov rdi, r10            ; First arg = map pointer

    ; Align stack to 16 bytes for C call
    mov rbp, rsp
    and rsp, -16

    ; Call hamt_free(map)
%ifdef PIC
    call hamt_free wrt ..plt
%else
    call hamt_free
%endif

    ; Restore stack
    mov rsp, rbp

    ; Restore VM registers
    pop rsi
    pop rdi

    ; No return value - just continue

    jmp vm_dispatch
