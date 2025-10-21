(* March Language Types and AST *)

(* Type system *)
type march_type =
  | TInt64
  | TBool
  | TPtr of march_type
  | TUnit
  | TFunc of march_type list * march_type list  (* (inputs, outputs) *)
  | TVar of string  (* Type variable for polymorphism *)
  | TUnknown

(* Stack effect: what a word consumes and produces *)
type stack_effect = {
  consumes: march_type list;
  produces: march_type list;
}

(* Cell tags for bytecode - Variable-bit encoding *)
type cell_tag =
  | TagXT     (* 00  - Execute word, or EXIT if addr=0 *)
  | TagLIT    (* 01  - Immediate 62-bit literal *)
  | TagLST    (* 10  - Symbol ID literal *)
  | TagLNT    (* 110 - Next N cells are raw literals *)
  | TagEXT    (* 111 - Future extension *)

(* Symbol table types *)
type symbol = {
  id: int64;
  name: string;
}

type symbol_table = (string, int64) Hashtbl.t

(* AST nodes *)
type expr =
  | ELit of int64                    (* Literal value *)
  | EWord of string                  (* Word reference *)
  | ECall of string * expr list      (* Function call *)
  | ESeq of expr list                (* Sequence of expressions *)

type word_def = {
  name: string;
  params: (string * march_type) list;  (* Parameter names and types *)
  returns: march_type list;            (* Return types *)
  body: expr list;                     (* Word body *)
  is_primitive: bool;                  (* Is this a primitive? *)
  doc: string option;                  (* Documentation *)
}

type program = {
  words: word_def list;
  exports: string list;
  imports: string list;
}

(* Cell encoding functions *)

(* Encode XT cell: address in upper 62 bits, tag=00 *)
let encode_xt addr =
  Int64.logand addr 0xFFFFFFFFFFFFFFFCL  (* Clear low 2 bits = 00 tag *)

(* Encode EXIT: just XT(0) *)
let encode_exit = 0L

(* Encode LIT: value in upper 62 bits (sign-extended), tag=01 *)
let encode_lit value =
  let shifted = Int64.shift_left value 2 in
  Int64.logor shifted 1L  (* Set tag = 01 *)

(* Encode LST: symbol ID in upper 62 bits, tag=10 *)
let encode_lst sym_id =
  let shifted = Int64.shift_left sym_id 2 in
  Int64.logor shifted 2L  (* Set tag = 10 *)

(* Encode LNT: count in upper 61 bits, tag=110 *)
let encode_lnt count =
  let shifted = Int64.shift_left count 3 in
  Int64.logor shifted 6L  (* Set tag = 110 *)

(* Encode EXT: value in upper 61 bits, tag=111 *)
let encode_ext value =
  let shifted = Int64.shift_left value 3 in
  Int64.logor shifted 7L  (* Set tag = 111 *)

(* Decode tag: read variable-bit tag *)
let decode_tag cell =
  let low2 = Int64.to_int (Int64.logand cell 3L) in
  match low2 with
  | 0 -> TagXT
  | 1 -> TagLIT
  | 2 ->
      (* Could be LST (10) or LNT (110) - check bit 2 *)
      let bit2 = Int64.to_int (Int64.logand (Int64.shift_right_logical cell 2) 1L) in
      if bit2 = 1 then TagLNT else TagLST
  | 3 ->
      (* Must be EXT (111) *)
      TagEXT
  | _ -> failwith "impossible"

(* Extract value from tagged cell *)
let decode_xt cell = Int64.logand cell 0xFFFFFFFFFFFFFFFCL
let decode_lit cell = Int64.shift_right cell 2  (* Sign-extending shift *)
let decode_lst cell = Int64.shift_right_logical cell 2  (* Unsigned shift *)
let decode_lnt cell = Int64.shift_right_logical cell 3
let decode_ext cell = Int64.shift_right_logical cell 3

(* Helper: check if address is 0 (EXIT) *)
let is_exit cell =
  let tag = decode_tag cell in
  tag = TagXT && (decode_xt cell) = 0L

(* Legacy helper for compatibility *)
let tag_to_int = function
  | TagXT -> 0
  | TagLIT -> 1
  | TagLST -> 2
  | TagLNT -> 6
  | TagEXT -> 7

let make_cell tag addr =
  match tag with
  | TagXT -> encode_xt addr
  | TagLIT -> encode_lit addr
  | TagLST -> encode_lst addr
  | TagLNT -> encode_lnt addr
  | TagEXT -> encode_ext addr

let rec string_of_type = function
  | TInt64 -> "int64"
  | TBool -> "bool"
  | TPtr t -> "ptr(" ^ string_of_type t ^ ")"
  | TUnit -> "unit"
  | TFunc (inputs, outputs) ->
      let ins = String.concat " " (List.map string_of_type inputs) in
      let outs = String.concat " " (List.map string_of_type outputs) in
      "(" ^ ins ^ " -> " ^ outs ^ ")"
  | TVar name -> "'" ^ name
  | TUnknown -> "?"
