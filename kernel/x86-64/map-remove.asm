; march.map.remove ( map key -- map' )
; Removes key from persistent hash map
; Returns new map (original unchanged)
; Returns original map if key not found

section .text
extern hamt_remove
extern vm_dispatch
global op_map_remove

op_map_remove:
    ; rsi = data stack pointer
    ; Stack layout: [key] [map] <- TOS (at rsi+8)

    ; Get arguments from data stack
    mov r10, [rsi]          ; key (TOS)
    mov r11, [rsi+8]        ; map (second item)
    add rsi, 16             ; Pop both arguments

    ; Save caller-saved VM registers
    push rdi
    push rsi

    ; Set up C function arguments
    ; void* hamt_remove(void* node, uint64_t key)
    mov rdi, r11            ; First arg = map pointer
    mov rsi, r10            ; Second arg = key

    ; Align stack to 16 bytes for C call
    mov rbp, rsp
    and rsp, -16

    ; Call hamt_remove(map, key)
%ifdef PIC
    call hamt_remove wrt ..plt
%else
    call hamt_remove
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
