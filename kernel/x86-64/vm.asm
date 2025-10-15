; March VM - Inner Interpreter
; Executes pre-compiled cell streams with tagged dispatch
;
; Register allocation:
;   rsi = Data stack pointer (grows down)
;   rdi = Return stack pointer (grows down)
;   rbx = Instruction pointer (IP) - points to current cell
;   rbp = VM context pointer (preserved)
;
; Cell encoding (64-bit):
;   Low 2 bits = tag:
;     00 = XT    (execute word - jump to address)
;     01 = EXIT  (return from word)
;     10 = LIT   (next cell is literal value)
;     11 = EXT   (extended instruction)

section .data
    align 8
    vm_running: dq 0        ; Flag: 1 if VM is running

section .bss
    align 16
    data_stack_base: resq 1024      ; Data stack (8KB)
    return_stack_base: resq 1024    ; Return stack (8KB)

section .text
    global vm_init
    global vm_run
    global vm_halt
    global vm_get_dsp
    global vm_get_rsp

; ============================================================================
; vm_init - Initialize the VM
; C signature: void vm_init(void)
; ============================================================================
vm_init:
    push rbp
    mov rbp, rsp

    ; Initialize data stack pointer (top of stack)
    lea rax, [data_stack_base]
    add rax, 8 * 1024           ; Point to end of stack area
    sub rax, 8                  ; Back up one slot
    mov [rel data_stack_top], rax

    ; Initialize return stack pointer
    lea rax, [return_stack_base]
    add rax, 8 * 1024
    sub rax, 8
    mov [rel return_stack_top], rax

    ; Clear running flag
    mov qword [rel vm_running], 0

    pop rbp
    ret

; ============================================================================
; vm_run - Execute a cell stream
; C signature: void vm_run(uint64_t* code_ptr)
; Arguments:
;   rdi = pointer to first cell of code stream
; ============================================================================
vm_run:
    push rbp
    mov rbp, rsp

    ; Save callee-saved registers
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; Set up VM registers
    mov rbx, rdi                        ; IP = code pointer (arg)
    mov rsi, [rel data_stack_top]       ; DSP = data stack top
    mov rdi, [rel return_stack_top]     ; RSP = return stack top

    ; Mark VM as running
    mov qword [rel vm_running], 1

    ; Fall through to dispatch loop

; ============================================================================
; Inner Interpreter - Main dispatch loop
; ============================================================================
.dispatch:
    ; Check if VM should halt
    mov rax, [rel vm_running]
    test rax, rax
    jz .halt

    ; Fetch next cell
    mov rcx, [rbx]              ; Load cell
    add rbx, 8                  ; Advance IP

    ; Extract tag (low 2 bits)
    mov rax, rcx
    and rax, 0x3                ; Isolate tag

    ; Dispatch on tag
    cmp rax, 0
    je .do_xt
    cmp rax, 1
    je .do_exit
    cmp rax, 2
    je .do_lit
    cmp rax, 3
    je .do_ext

    ; Should never reach here
    jmp .error

; ----------------------------------------------------------------------------
; XT (00) - Execute word at address
; ----------------------------------------------------------------------------
.do_xt:
    ; Clear tag bits to get address
    and rcx, ~0x3               ; Mask off low 2 bits

    ; Save IP on return stack and jump to word
    sub rdi, 8                  ; Allocate return stack slot
    mov [rdi], rbx              ; Save IP

    ; Jump to the word
    call rcx                    ; Call the machine code

    ; Restore IP from return stack
    mov rbx, [rdi]              ; Load IP
    add rdi, 8                  ; Drop return stack slot

    jmp .dispatch

; ----------------------------------------------------------------------------
; EXIT (01) - Return from word
; ----------------------------------------------------------------------------
.do_exit:
    ; Check if return stack is at base (we're done)
    lea rax, [return_stack_base]
    add rax, 8 * 1024
    sub rax, 8
    cmp rdi, rax
    jge .halt                   ; Return stack empty, halt VM

    ; Pop IP from return stack
    mov rbx, [rdi]              ; Load saved IP
    add rdi, 8                  ; Drop from return stack

    jmp .dispatch

; ----------------------------------------------------------------------------
; LIT (10) - Push literal value
; ----------------------------------------------------------------------------
.do_lit:
    ; Next cell contains the literal value
    mov rax, [rbx]              ; Load literal
    add rbx, 8                  ; Advance IP

    ; Push to data stack
    sub rsi, 8                  ; Allocate space
    mov [rsi], rax              ; Store literal

    jmp .dispatch

; ----------------------------------------------------------------------------
; EXT (11) - Extended instruction
; ----------------------------------------------------------------------------
.do_ext:
    ; Next cell contains the extended opcode
    mov rax, [rbx]              ; Load extended opcode
    add rbx, 8                  ; Advance IP

    ; For now, just ignore (future extension point)
    ; TODO: Implement extended instruction dispatch

    jmp .dispatch

; ----------------------------------------------------------------------------
; Error handling
; ----------------------------------------------------------------------------
.error:
    ; Invalid tag - halt VM
    mov qword [rel vm_running], 0
    ; Fall through to halt

; ----------------------------------------------------------------------------
; Halt VM and return to caller
; ----------------------------------------------------------------------------
.halt:
    ; Mark VM as stopped
    mov qword [rel vm_running], 0

    ; Save final stack pointers
    mov [rel data_stack_top], rsi
    mov [rel return_stack_top], rdi

    ; Restore callee-saved registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx

    pop rbp
    ret

; ============================================================================
; vm_halt - Stop the VM
; C signature: void vm_halt(void)
; ============================================================================
vm_halt:
    mov qword [rel vm_running], 0
    ret

; ============================================================================
; vm_get_dsp - Get current data stack pointer
; C signature: uint64_t* vm_get_dsp(void)
; ============================================================================
vm_get_dsp:
    mov rax, [rel data_stack_top]
    ret

; ============================================================================
; vm_get_rsp - Get current return stack pointer
; C signature: uint64_t* vm_get_rsp(void)
; ============================================================================
vm_get_rsp:
    mov rax, [rel return_stack_top]
    ret

; ============================================================================
; Data section for stack tops
; ============================================================================
section .data
    align 8
    data_stack_top: dq 0
    return_stack_top: dq 0
