; alloc ( size -- ptr )
; Allocates memory of the given size in bytes
; Calls C malloc() and returns pointer

section .text
extern malloc              ; C stdlib malloc
extern vm_dispatch
global op_alloc

op_alloc:
    ; rsi = data stack pointer (grows downward)
    ; rdi = return stack pointer (grows downward)
    ; rbx = instruction pointer
    ; TOS contains size in bytes

    ; Get size argument from data stack
    mov r10, [rsi]          ; Load size from TOS into temp register
    add rsi, 8              ; Pop size from data stack

    ; Save caller-saved VM registers (rbx is callee-saved, so malloc won't touch it)
    push rdi                ; Save RSP (return stack pointer) - caller-saved
    push rsi                ; Save DSP (already adjusted) - caller-saved
    ; Note: rbx (IP) is callee-saved, no need to save it

    ; Set up C function argument
    mov rdi, r10            ; First arg = size

    ; Align stack to 16 bytes for C call (System V ABI requirement)
    ; We've pushed 16 bytes (2 qwords), but need to ensure alignment
    mov rbp, rsp
    and rsp, -16

    ; Call malloc(size) via PLT (for PIC compatibility)
%ifdef PIC
    call malloc wrt ..plt   ; Result in rax (PIC)
%else
    call malloc             ; Result in rax (non-PIC)
%endif

    ; Restore stack
    mov rsp, rbp

    ; Restore VM registers
    pop rsi                 ; Restore DSP
    pop rdi                 ; Restore RSP

    ; Push pointer result to data stack
    sub rsi, 8              ; Allocate space on stack
    mov [rsi], rax          ; Store pointer (might be NULL if malloc failed)

    jmp vm_dispatch         ; Return to VM dispatch
