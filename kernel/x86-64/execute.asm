; execute ( addr -- )
; Pops an address from the stack and executes it as a word
; The address should point to executable machine code (like a quotation)
; Stack effect: Pop address, execute word at that address

section .text
extern vm_dispatch
global op_execute

op_execute:
    ; rsi = data stack pointer (grows downward)
    ; rdi = return stack pointer (grows downward)
    ; rbx = IP (instruction pointer)

    ; Pop address from data stack
    mov rax, [rsi]          ; Load address from TOS
    add rsi, 8              ; Drop from data stack

    ; Check for null address (safety)
    test rax, rax
    jz .done                ; If null, just return

    ; FORTH-style execute: just jump to the address (direct threading)
    ; The address is a DOCOL wrapper that will:
    ;  1. Save current IP on return stack
    ;  2. Set IP to the quotation's cells
    ;  3. Jump to vm_dispatch
    ; When the quotation hits EXIT, it will restore IP and continue
    jmp rax

.done:
    jmp vm_dispatch         ; Return to VM dispatch (FORTH-style)
