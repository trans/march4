; march.array.reverse! ( array! -- array! )
; Reverse array elements in place
; Returns array pointer for chaining

section .text
extern vm_dispatch
global op_array_reverse

op_array_reverse:
    ; rsi = data stack pointer
    ; Stack: array_ptr

    ; Get array pointer
    mov rax, [rsi]          ; rax = array_ptr

    ; Read count from header (offset 0)
    mov rcx, [rax]          ; rcx = count

    ; If count <= 1, nothing to reverse
    cmp rcx, 1
    jle .done

    ; Set up pointers
    ; left = array_ptr + 32 (first element)
    ; right = array_ptr + 32 + (count-1)*8 (last element)
    lea rdi, [rax + 32]     ; rdi = left pointer
    dec rcx                 ; rcx = count - 1
    lea r8, [rax + 32 + rcx * 8]  ; r8 = right pointer

    ; Swap elements from outside in
.swap_loop:
    cmp rdi, r8             ; If left >= right, we're done
    jge .done

    ; Swap [rdi] and [r8]
    mov r9, [rdi]           ; r9 = left value
    mov r10, [r8]           ; r10 = right value
    mov [rdi], r10          ; left = right value
    mov [r8], r9            ; right = left value

    ; Move pointers toward center
    add rdi, 8              ; left++
    sub r8, 8               ; right--
    jmp .swap_loop

.done:
    ; array_ptr already on stack at [rsi]
    jmp vm_dispatch
