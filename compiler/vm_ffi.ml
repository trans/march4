(* March VM FFI - OCaml interface to assembly VM *)

(* Initialize the VM (sets up stack pointers) *)
external vm_init : unit -> unit = "caml_vm_init"

(* Execute a cell stream *)
external vm_run : int64 array -> unit = "caml_vm_run"

(* Get current data stack contents as array *)
external vm_get_stack : unit -> int64 array = "caml_vm_get_stack"

(* Halt the VM *)
external vm_halt : unit -> unit = "caml_vm_halt"

(* High-level helper: execute code and return stack *)
let execute cells =
  vm_init ();
  vm_run cells;
  vm_get_stack ()
