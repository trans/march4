(* March Language Parser *)

open Types
open Lexer

exception ParseError of string

(* Parser state *)
type parser_state = {
  tokens: token list;
  pos: int;
}

let peek state =
  if state.pos < List.length state.tokens then
    List.nth state.tokens state.pos
  else TokEOF

let advance state =
  { state with pos = state.pos + 1 }

let expect token state =
  let current = peek state in
  if current = token then advance state
  else raise (ParseError (Printf.sprintf "Expected %s but got %s"
    (string_of_token token) (string_of_token current)))

(* Parse a single expression *)
let parse_expr state =
  match peek state with
  | TokInt n ->
      (ELit n, advance state)

  | TokWord w ->
      (EWord w, advance state)

  | TokEOF | TokSemicolon ->
      raise (ParseError "Unexpected end of expression")

  | tok ->
      raise (ParseError (Printf.sprintf "Unexpected token in expression: %s"
        (string_of_token tok)))

(* Parse sequence of expressions until semicolon *)
let rec parse_expr_seq state acc =
  match peek state with
  | TokSemicolon | TokEOF ->
      (List.rev acc, state)
  | _ ->
      let (expr, state') = parse_expr state in
      parse_expr_seq state' (expr :: acc)

(* Parse type signature: type1 type2 ... -> type1 type2 ... *)
let rec parse_type_list state acc =
  match peek state with
  | TokWord "int64" ->
      parse_type_list (advance state) (TInt64 :: acc)
  | TokWord "bool" ->
      parse_type_list (advance state) (TBool :: acc)
  | TokWord "unit" ->
      parse_type_list (advance state) (TUnit :: acc)
  | TokArrow | TokSemicolon | TokEOF ->
      (List.rev acc, state)
  | tok ->
      raise (ParseError (Printf.sprintf "Expected type, got %s" (string_of_token tok)))

let parse_signature state =
  let (inputs, state') = parse_type_list state [] in
  if peek state' = TokArrow then
    let state'' = advance state' in
    let (outputs, state''') = parse_type_list state'' [] in
    (inputs, outputs, state''')
  else
    (* No arrow means no inputs, only outputs *)
    ([], inputs, state')

(* Parse word definition: : name [type sig] body ; *)
let parse_word_def state =
  let state = expect TokColon state in

  (* Get word name *)
  let (name, state) = match peek state with
    | TokWord n -> (n, advance state)
    | _ -> raise (ParseError "Expected word name after ':'")
  in

  (* Try to parse type signature (optional for now) *)
  let (_inputs, outputs, state) =
    match peek state with
    | TokLParen ->
        let state = advance state in
        let (ins, outs, state) = parse_signature state in
        let state = expect TokRParen state in
        (ins, outs, state)
    | _ -> ([], [], state)  (* No signature *)
  in

  (* Parse body until semicolon *)
  let (body, state) = parse_expr_seq state [] in
  let state = expect TokSemicolon state in

  let word = {
    name;
    params = [];  (* TODO: named parameters *)
    returns = outputs;
    body;
    is_primitive = false;
    doc = None;
  } in
  (word, state)

(* Parse entire program *)
let rec parse_program state acc =
  match peek state with
  | TokEOF ->
      { words = List.rev acc; exports = []; imports = [] }

  | TokColon ->
      let (word, state') = parse_word_def state in
      parse_program state' (word :: acc)

  | tok ->
      Printf.eprintf "Warning: skipping unexpected token: %s\n" (string_of_token tok);
      parse_program (advance state) acc

let parse tokens =
  let state = { tokens; pos = 0 } in
  parse_program state []
