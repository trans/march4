; march.map.set ( map key value -- map' )
; Sets key to value in persistent hash map
; Returns new map (original unchanged due to persistence)

section .text
extern hamt_set
extern vm_dispatch
global op_map_set

op_map_set:
    ; rsi = data stack pointer
    ; Stack layout: [value] [key] [map] <- TOS (at rsi+16)

    ; Get arguments from data stack
    mov r10, [rsi]          ; value (TOS)
    mov r11, [rsi+8]        ; key (second item)
    mov r12, [rsi+16]       ; map (third item)
    add rsi, 24             ; Pop all three arguments

    ; Save caller-saved VM registers
    push rdi
    push rsi

    ; Set up C function arguments
    ; void* hamt_set(void* node, uint64_t key, uint64_t value)
    mov rdi, r12            ; First arg = map pointer
    mov rsi, r11            ; Second arg = key
    mov rdx, r10            ; Third arg = value

    ; Align stack to 16 bytes for C call
    mov rbp, rsp
    and rsp, -16

    ; Call hamt_set(map, key, value)
%ifdef PIC
    call hamt_set wrt ..plt
%else
    call hamt_set
%endif

    ; Restore stack
    mov rsp, rbp

    ; Restore VM registers
    pop rsi
    pop rdi

    ; Push new map pointer to data stack
    sub rsi, 8
    mov [rsi], rax          ; Store new map pointer

    jmp vm_dispatch
