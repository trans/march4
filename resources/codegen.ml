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

(* Emit EXIT instruction - now XT(0) *)
let emit_exit ctx =
  emit_cell ctx Types.encode_exit

(* Emit LIT instruction - single cell with embedded 62-bit value *)
let emit_lit ctx value =
  emit_cell ctx (Types.encode_lit value)

(* Emit XT (execute word) instruction *)
let emit_xt ctx addr =
  emit_cell ctx (Types.encode_xt addr)

(* Emit symbol literal *)
let emit_symbol ctx sym_id =
  emit_cell ctx (Types.encode_lst sym_id)

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
      gen_expr_list ctx exprs

(* Generate code for a list of expressions with LNT optimization *)
and gen_expr_list ctx exprs =
  match exprs with
  | [] -> ctx
  | ELit v1 :: ELit v2 :: rest ->
      (* Found literal sequence, collect them *)
      let rec collect_lits acc = function
        | ELit v :: rest -> collect_lits (v :: acc) rest
        | rest -> (List.rev acc, rest)
      in
      (* Start accumulator with v2, v1 (reversed) so after collecting and reversing we get correct order *)
      let (lits, remaining) = collect_lits [v2; v1] rest in
      let count = List.length lits in
      (* Use LNT for 1+ consecutive literals *)
      (* Note: LNT allows full 64-bit values, while LIT is limited to 62-bit signed *)
      (* TODO: Check if values fit in 62-bit and optimize accordingly *)
      if count >= 1 then begin
        Printf.eprintf "DEBUG: LNT optimization: %d literals: %s\n"
          count
          (String.concat ", " (List.map Int64.to_string lits));
        let ctx = emit_cell ctx (Types.encode_lnt (Int64.of_int count)) in
        let ctx = List.fold_left (fun c v -> emit_cell c v) ctx lits in
        gen_expr_list ctx remaining
      end else
        (* Emit as individual LIT cells *)
        let ctx = List.fold_left emit_lit ctx lits in
        gen_expr_list ctx remaining
  | expr :: rest ->
      let ctx = gen_expr ctx expr in
      gen_expr_list ctx rest

(* Generate code for a word definition *)
let gen_word_def ctx word =
  (* Generate body using gen_expr_list for LNT optimization *)
  let ctx = gen_expr_list ctx word.body in
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
let compute_cid bytes =
  Digestif.SHA256.(to_hex (digest_string bytes))

(* Generate code and compute CIDs *)
let generate program =
  let word_code_list = gen_program program in
  List.map (fun (name, cells) ->
    let bytes = cells_to_bytes cells in
    let cid = compute_cid bytes in
    (name, cid, bytes, cells)
  ) word_code_list
