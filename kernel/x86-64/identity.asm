; _ ( a -- a )
; Identity function - does nothing, just passes value through
; Stack effect: None (value remains on stack)

section .text
extern vm_dispatch
global op_identity

op_identity:
    ; rsi = data stack pointer (grows downward)
    ; Stack layout: [TOS] <- rsi points here
    ; Do nothing - just pass through to next instruction
    jmp vm_dispatch         ; Return to VM dispatch (FORTH-style)
