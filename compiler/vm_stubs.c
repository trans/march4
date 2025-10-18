/*
 * March VM FFI - C stubs for OCaml
 * Provides OCaml bindings to the assembly VM
 */

#include <stdint.h>
#include <string.h>
#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/alloc.h>
#include <caml/fail.h>

/* External VM functions from assembly */
extern void vm_init(void);
extern void vm_run(uint64_t* code_ptr);
extern void vm_halt(void);
extern uint64_t* vm_get_dsp(void);
extern uint64_t* vm_get_rsp(void);

/* Helper: get data stack base pointer (defined in vm.asm) */
extern uint64_t data_stack_base[1024];

/*
 * vm_init wrapper
 * OCaml signature: unit -> unit
 */
CAMLprim value caml_vm_init(value unit) {
    CAMLparam1(unit);
    vm_init();
    CAMLreturn(Val_unit);
}

/*
 * vm_run wrapper
 * OCaml signature: int64 array -> unit
 *
 * Note: We need to copy the OCaml array to C memory because:
 * 1. OCaml arrays have a header (not raw uint64_t*)
 * 2. VM expects a raw pointer to cells
 * 3. VM may modify IP which should not affect OCaml memory
 */
CAMLprim value caml_vm_run(value cells_array) {
    CAMLparam1(cells_array);

    /* Get array length */
    mlsize_t len = Wosize_val(cells_array);

    /* Allocate C buffer for cells */
    uint64_t* code = (uint64_t*)malloc(len * sizeof(uint64_t));
    if (code == NULL) {
        caml_failwith("vm_run: failed to allocate memory for code buffer");
    }

    /* Copy cells from OCaml array to C buffer */
    for (mlsize_t i = 0; i < len; i++) {
        code[i] = (uint64_t)Int64_val(Field(cells_array, i));
    }

    /* Execute */
    vm_run(code);

    /* Clean up */
    free(code);

    CAMLreturn(Val_unit);
}

/*
 * vm_get_stack wrapper
 * OCaml signature: unit -> int64 array
 *
 * Returns the current data stack contents.
 * Stack grows downward, so we need to calculate how many items are on it.
 */
CAMLprim value caml_vm_get_stack(value unit) {
    CAMLparam1(unit);
    CAMLlocal1(result);

    /* Get current data stack pointer */
    uint64_t* dsp = vm_get_dsp();

    /* Calculate stack depth
     * Stack base is at data_stack_base + (8KB - 8)
     * Stack pointer grows downward
     * Depth = (base - current) / 8
     */
    uint64_t* base = data_stack_base + 1024 - 1;
    ptrdiff_t depth = base - dsp;

    if (depth < 0) {
        depth = 0;  /* Stack underflow protection */
    }

    /* Allocate OCaml array */
    result = caml_alloc(depth, 0);  /* 0 = regular array tag */

    /* Copy stack contents to array
     * Stack item 0 is at dsp[0] (top)
     * Stack item 1 is at dsp[1]
     * etc.
     */
    for (ptrdiff_t i = 0; i < depth; i++) {
        Store_field(result, i, caml_copy_int64((int64_t)dsp[i]));
    }

    CAMLreturn(result);
}

/*
 * vm_halt wrapper
 * OCaml signature: unit -> unit
 */
CAMLprim value caml_vm_halt(value unit) {
    CAMLparam1(unit);
    vm_halt();
    CAMLreturn(Val_unit);
}
