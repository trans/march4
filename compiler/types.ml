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

(* Cell tags for bytecode *)
type cell_tag =
  | TagXT     (* 00 - Execute word *)
  | TagExit   (* 01 - Return *)
  | TagLit    (* 10 - Literal *)
  | TagExt    (* 11 - Extended *)

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

(* Helper functions *)
let tag_to_int = function
  | TagXT -> 0
  | TagExit -> 1
  | TagLit -> 2
  | TagExt -> 3

let make_cell tag addr =
  Int64.logor (Int64.of_int (tag_to_int tag)) (Int64.logand addr 0xFFFFFFFFFFFFFFFCL)

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
