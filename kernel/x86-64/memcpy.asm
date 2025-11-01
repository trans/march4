; memcpy ( src dest len -- dest )
; Copy len bytes from src to dest
; Returns dest pointer
; Uses movsb for byte-by-byte copy (simple, correct)

section .text
extern vm_dispatch
global op_memcpy

op_memcpy:
    ; rsi = data stack pointer (grows downward)
    ; Stack layout: [len] [dest] [src] <- rsi points to src

    mov rcx, [rsi + 16]     ; rcx = len (count for rep movsb)
    mov rdi, [rsi + 8]      ; rdi = dest
    mov rax, rdi            ; rax = dest (save for return value)
    mov r10, rsi            ; r10 = save data stack pointer
    mov rsi, [rsi]          ; rsi = src (for movsb)

    ; Copy bytes: rep movsb copies rcx bytes from [rsi] to [rdi]
    rep movsb

    ; Restore data stack pointer and clean up
    mov rsi, r10            ; Restore data stack pointer
    add rsi, 16             ; Pop src and len (2 items)
    mov [rsi], rax          ; Replace dest with dest (already there, but ensure)

    jmp vm_dispatch
