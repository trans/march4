; march.array.concat ( array1 array2 -- array )
; Concatenate two arrays into a new array
; Returns new immutable array containing all elements

section .text
extern malloc
extern vm_dispatch
global op_array_concat

op_array_concat:
    ; rsi = data stack pointer
    ; Stack: array1_ptr array2_ptr

    ; Save data stack pointer in callee-saved register
    push rbx                ; Save rbx (instruction pointer - will restore)
    mov rbx, rsi            ; rbx = saved DSP

    ; Get array pointers
    mov r12, [rbx + 8]      ; r12 = array1_ptr
    mov r13, [rbx]          ; r13 = array2_ptr

    ; Read counts from headers
    mov r14, [r12]          ; r14 = count1
    mov r15, [r13]          ; r15 = count2

    ; Calculate total count
    mov rax, r14
    add rax, r15            ; rax = count1 + count2 (total_count)
    push rax                ; Save total_count

    ; Calculate total size: 32 + total_count * 8
    shl rax, 3              ; rax = total_count * 8
    add rax, 32             ; rax = total_size

    ; Call malloc
    push rdi                ; Save RSP
    mov rdi, rax            ; First arg = total_size

    ; Align stack
    mov rbp, rsp
    and rsp, -16

%ifdef PIC
    call malloc wrt ..plt
%else
    call malloc
%endif

    mov rsp, rbp
    pop rdi                 ; Restore RSP
    pop rcx                 ; rcx = total_count

    ; rax = new_array_ptr
    test rax, rax
    jz .malloc_failed

    ; Write header
    mov [rax], rcx          ; count
    mov byte [rax + 8], 8   ; elem_size
    mov r8, [r12 + 16]      ; elem_type from array1
    mov [rax + 16], r8

    ; Copy from array1
    mov rdi, rax
    add rdi, 32             ; rdi = dest
    mov rsi, r12
    add rsi, 32             ; rsi = source (array1 data)
    mov rcx, r14            ; rcx = count1

.copy1:
    test rcx, rcx
    jz .copy2_setup
    mov r8, [rsi]
    mov [rdi], r8
    add rsi, 8
    add rdi, 8
    dec rcx
    jmp .copy1

.copy2_setup:
    ; Copy from array2
    mov rsi, r13
    add rsi, 32             ; rsi = source (array2 data)
    mov rcx, r15            ; rcx = count2

.copy2:
    test rcx, rcx
    jz .done
    mov r8, [rsi]
    mov [rdi], r8
    add rsi, 8
    add rdi, 8
    dec rcx
    jmp .copy2

.done:
    ; Restore DSP and return result
    mov rsi, rbx            ; rsi = restored DSP
    add rsi, 8              ; Pop array2
    mov [rsi], rax          ; Replace array1 with new array
    pop rbx                 ; Restore original rbx
    jmp vm_dispatch

.malloc_failed:
    mov rsi, rbx
    add rsi, 8
    xor rax, rax
    mov [rsi], rax
    pop rbx
    jmp vm_dispatch
