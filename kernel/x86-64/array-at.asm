; march.array.at ( array index -- value )
; Read element at index from array
; Performs bounds checking, returns element value

section .text
extern vm_dispatch
global op_array_at

op_array_at:
    ; rsi = data stack pointer
    ; rdi = return stack pointer
    ; rbx = instruction pointer
    ; Stack: array_ptr index

    ; Get index and array pointer
    mov rcx, [rsi]          ; rcx = index
    mov rax, [rsi + 8]      ; rax = array_ptr

    ; Read count from header (offset 0)
    mov rdx, [rax]          ; rdx = count

    ; Bounds check: index must be >= 0 and < count
    test rcx, rcx           ; Check if index < 0 (signed)
    js .bounds_error
    cmp rcx, rdx            ; Compare index with count
    jge .bounds_error       ; if index >= count, error

    ; Calculate element offset: 32 + (index * 8)
    mov r8, rcx             ; r8 = index
    shl r8, 3               ; r8 = index * 8
    add r8, 32              ; r8 = 32 + (index * 8)

    ; Read element at offset
    mov rax, [rax + r8]     ; rax = array[index]

    ; Pop index and array from stack, push result
    add rsi, 8              ; Pop index (keep array slot)
    mov [rsi], rax          ; Replace array with value

    jmp vm_dispatch

.bounds_error:
    ; For now, just return 0 on bounds error
    ; TODO: Proper error handling
    add rsi, 8              ; Pop index
    mov qword [rsi], 0      ; Replace array with 0
    jmp vm_dispatch
