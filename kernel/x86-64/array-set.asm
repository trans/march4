; march.array.set! ( array! index value -- array! )
; Write value to array at index (mutates array in-place)
; Returns the array pointer for chaining operations
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

    ; Pop index and value, leave array on stack
    add rsi, 16             ; Pop value and index (2 * 8 bytes)
    ; array_ptr is already at [rsi] (unchanged)

    jmp vm_dispatch

.bounds_error:
    ; For now, just ignore the write on bounds error
    ; TODO: Proper error handling
    add rsi, 16             ; Pop value and index, leave array
    jmp vm_dispatch
