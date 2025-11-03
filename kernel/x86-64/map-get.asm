; march.map.get ( map key -- value )
; Gets value for key from persistent hash map
; Returns 0 if key not found

section .text
extern hamt_get
extern vm_dispatch
global op_map_get

op_map_get:
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
    ; uint64_t hamt_get(void* node, uint64_t key)
    mov rdi, r11            ; First arg = map pointer
    mov rsi, r10            ; Second arg = key

    ; Align stack to 16 bytes for C call
    mov rbp, rsp
    and rsp, -16

    ; Call hamt_get(map, key)
%ifdef PIC
    call hamt_get wrt ..plt
%else
    call hamt_get
%endif

    ; Restore stack
    mov rsp, rbp

    ; Restore VM registers
    pop rsi
    pop rdi

    ; Push value result to data stack
    sub rsi, 8
    mov [rsi], rax          ; Store value (0 if not found)

    jmp vm_dispatch
