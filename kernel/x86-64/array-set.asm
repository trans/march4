; march.array.set! ( array! index value -- )
; Write value to array at index (mutates array in-place)
; Requires mutable array (array!), performs bounds checking

section .text
extern vm_dispatch
global op_array_set

op_array_set:
    ; rsi = data stack pointer
    ; rdi = return stack pointer
    ; rbx = instruction pointer
    ; Stack: array_ptr index value

    ; Get value, index, and array pointer
    mov r8, [rsi]           ; r8 = value
    mov rcx, [rsi + 8]      ; rcx = index
    mov rax, [rsi + 16]     ; rax = array_ptr

    ; Read count from header (offset 0)
    mov rdx, [rax]          ; rdx = count

    ; Bounds check: index must be >= 0 and < count
    test rcx, rcx           ; Check if index < 0 (signed)
    js .bounds_error
    cmp rcx, rdx            ; Compare index with count
    jge .bounds_error       ; if index >= count, error

    ; Calculate element offset: 32 + (index * 8)
    mov r9, rcx             ; r9 = index
    shl r9, 3               ; r9 = index * 8
    add r9, 32              ; r9 = 32 + (index * 8)

    ; Write value to array at offset
    mov [rax + r9], r8      ; array[index] = value

    ; Pop all three arguments from stack
    add rsi, 24             ; Pop value, index, array (3 * 8 bytes)

    jmp vm_dispatch

.bounds_error:
    ; For now, just ignore the write on bounds error
    ; TODO: Proper error handling
    add rsi, 24             ; Pop value, index, array
    jmp vm_dispatch
