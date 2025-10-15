(* March Code Generator - Emit Cell Streams *)

open Types

(* Code generation context *)
type codegen_ctx = {
  cells: int64 list;              (* Accumulated cells *)
  word_addresses: (string, int64) Hashtbl.t;  (* Word name -> address map *)
}

let create_ctx () = {
  cells = [];
  word_addresses = Hashtbl.create 100;
}

(* Emit a cell *)
let emit_cell ctx cell =
  { ctx with cells = cell :: ctx.cells }

(* Emit EXIT instruction *)
let emit_exit ctx =
  emit_cell ctx 1L  (* TagExit = 01 *)

(* Emit LIT instruction + value *)
let emit_lit ctx value =
  let ctx = emit_cell ctx 2L in  (* TagLit = 10 *)
  emit_cell ctx value

(* Emit XT (execute word) instruction *)
let emit_xt ctx addr =
  (* Tag = 00, addr must be aligned *)
  let tagged = Int64.logor (Int64.logand addr 0xFFFFFFFFFFFFFFFCL) 0L in
  emit_cell ctx tagged

(* Primitive word addresses (these will be linked later) *)
(* For now, we'll use placeholder addresses that will be patched during linking *)
let get_primitive_addr name =
  (* Hash the name to get a pseudo-address *)
  (* In real implementation, this comes from the database *)
  Int64.of_int (Hashtbl.hash name)

(* Generate code for an expression *)
let rec gen_expr ctx = function
  | ELit n ->
      emit_lit ctx n

  | EWord name ->
      (* Look up word address or treat as primitive *)
      let addr =
        if Hashtbl.mem ctx.word_addresses name then
          Hashtbl.find ctx.word_addresses name
        else
          get_primitive_addr name
      in
      emit_xt ctx addr

  | ECall (name, args) ->
      (* Generate code for arguments first *)
      let ctx = List.fold_left gen_expr ctx args in
      (* Then call the word *)
      let addr = get_primitive_addr name in
      emit_xt ctx addr

  | ESeq exprs ->
      List.fold_left gen_expr ctx exprs

(* Generate code for a word definition *)
let gen_word_def ctx word =
  (* Generate body *)
  let ctx = List.fold_left gen_expr ctx word.body in
  (* Add EXIT at end *)
  let ctx = emit_exit ctx in

  (* Get the code as byte array *)
  let cells = List.rev ctx.cells in
  (cells, ctx)

(* Generate code for entire program *)
let gen_program program =
  let ctx = create_ctx () in

  (* Generate code for each word *)
  let gen_one_word (word_code_list, ctx) word =
    let (cells, ctx) = gen_word_def { ctx with cells = [] } word in
    ((word.name, cells) :: word_code_list, ctx)
  in

  let (word_code_list, _) = List.fold_left gen_one_word ([], ctx) program.words in
  List.rev word_code_list

(* Convert cells to byte array *)
let cells_to_bytes cells =
  let buf = Buffer.create (List.length cells * 8) in
  List.iter (fun cell ->
    (* Write as little-endian 64-bit int *)
    for i = 0 to 7 do
      let byte = Int64.to_int (Int64.shift_right_logical cell (i * 8)) land 0xFF in
      Buffer.add_char buf (Char.chr byte)
    done
  ) cells;
  Buffer.contents buf

(* Compute SHA256 hash (content ID) *)
(* Note: This is a placeholder - in production use a real SHA256 library *)
let compute_cid bytes =
  let hash = Hashtbl.hash bytes in
  Printf.sprintf "%016x%016x%016x%016x" hash hash hash hash

(* Generate code and compute CIDs *)
let generate program =
  let word_code_list = gen_program program in
  List.map (fun (name, cells) ->
    let bytes = cells_to_bytes cells in
    let cid = compute_cid bytes in
    (name, cid, bytes, cells)
  ) word_code_list
