; march.array.fill! ( array! value -- array! )
; Fill all elements of array with the given value
; Returns array pointer for chaining

section .text
extern vm_dispatch
global op_array_fill

op_array_fill:
    ; rsi = data stack pointer
    ; Stack: array_ptr value

    ; Get value and array pointer
    mov r8, [rsi]           ; r8 = value
    mov rax, [rsi + 8]      ; rax = array_ptr

    ; Read count from header (offset 0)
    mov rcx, [rax]          ; rcx = count (number of elements)

    ; Calculate starting address: array_ptr + 32
    lea rdi, [rax + 32]     ; rdi = pointer to first element

    ; Fill loop
.fill_loop:
    test rcx, rcx           ; Check if count == 0
    jz .done

    mov [rdi], r8           ; Store value at current position
    add rdi, 8              ; Move to next element (8 bytes)
    dec rcx                 ; Decrement counter
    jmp .fill_loop

.done:
    ; Pop value, leave array on stack
    add rsi, 8              ; Pop value
    ; array_ptr already at [rsi]

    jmp vm_dispatch
