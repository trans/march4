; free ( slot_id -- )
; Frees memory at the given slot index
; TODO: Implement actual slot-based deallocation
; For now, this is a stub that just pops the slot_id

section .text
extern vm_dispatch
global op_free

op_free:
    ; rsi = data stack pointer
    ; TOS contains slot_id (for now, just drop it)

    add rsi, 8              ; Pop slot_id from stack

    ; TODO: Implement runtime slot array and free(slots[slot_id])
    ; This requires runtime infrastructure that doesn't exist yet.
    ; For now, this is a no-op.

    jmp vm_dispatch         ; Return to VM dispatch
