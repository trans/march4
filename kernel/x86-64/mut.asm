; mut ( ref -- ref' )
; Creates a mutable copy of an array or string
; Reads the header to determine size, allocates new memory, copies all bytes

section .text
extern malloc              ; C stdlib malloc
extern vm_dispatch
global op_mut

op_mut:
    ; rsi = data stack pointer
    ; rdi = return stack pointer
    ; rbx = instruction pointer
    ; TOS contains reference pointer (array or string)

    ; Get ref pointer from stack
    mov rax, [rsi]          ; rax = ref_ptr

    ; Read header to calculate total size
    ; Header layout: [count: u64][elem_size: u8][padding: 7][elem_type: u64][reserved: u64]
    mov rcx, [rax]          ; rcx = count (offset 0)
    movzx rdx, byte [rax + 8]  ; rdx = elem_size (offset 8)
    imul rcx, rdx           ; rcx = count * elem_size (data bytes)
    add rcx, 32             ; rcx = total_size (32-byte header + data)

    ; Save values we need
    push rcx                ; Save total_size
    push rax                ; Save old ref_ptr
    push rdi                ; Save RSP
    push rsi                ; Save DSP

    ; Call malloc to allocate new memory
    mov rdi, rcx            ; First arg = total_size

    ; Align stack to 16 bytes for C call
    mov rbp, rsp
    and rsp, -16

%ifdef PIC
    call malloc wrt ..plt
%else
    call malloc
%endif

    ; Restore stack
    mov rsp, rbp

    ; Restore saved values
    pop rsi                 ; Restore DSP
    pop rdi                 ; Restore RSP
    pop r8                  ; r8 = old ref_ptr (source)
    pop rcx                 ; rcx = total_size

    ; Now copy bytes from old to new
    ; rax = new_ptr (destination, from malloc)
    ; r8 = old_ptr (source)
    ; rcx = total_size (bytes to copy)

    test rax, rax           ; Check if malloc succeeded
    jz .malloc_failed

    ; Save registers for rep movsb
    mov r9, rax             ; r9 = new_ptr (save for return value)
    push rsi                ; Save DSP again (rep movsb uses rsi)

    ; Set up for rep movsb
    mov rdi, rax            ; rdi = destination (new_ptr)
    mov rsi, r8             ; rsi = source (old_ptr)
    ; rcx already has byte count
    rep movsb               ; Copy rcx bytes from [rsi] to [rdi]

    ; Restore DSP
    pop rsi

    ; Replace old ref_ptr with new ref_ptr on data stack
    mov [rsi], r9           ; Store new_ptr on TOS

    jmp vm_dispatch

.malloc_failed:
    ; If malloc failed, return NULL (0)
    mov [rsi], rax          ; rax is already 0 from malloc failure
    jmp vm_dispatch
