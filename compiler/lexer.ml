(* March Language Lexer *)

type token =
  | TokInt of int64
  | TokWord of string
  | TokColon          (* : *)
  | TokSemicolon      (* ; *)
  | TokArrow          (* -> *)
  | TokLParen         (* ( *)
  | TokRParen         (* ) *)
  | TokLBracket       (* [ *)
  | TokRBracket       (* ] *)
  | TokComment of string
  | TokEOF

let string_of_token = function
  | TokInt n -> Printf.sprintf "INT(%Ld)" n
  | TokWord s -> Printf.sprintf "WORD(%s)" s
  | TokColon -> ":"
  | TokSemicolon -> ";"
  | TokArrow -> "->"
  | TokLParen -> "("
  | TokRParen -> ")"
  | TokLBracket -> "["
  | TokRBracket -> "]"
  | TokComment s -> Printf.sprintf "COMMENT(%s)" s
  | TokEOF -> "EOF"

(* Simple lexer - breaks input into tokens *)
let tokenize input =
  let len = String.length input in
  let rec scan pos acc =
    if pos >= len then List.rev (TokEOF :: acc)
    else
      let c = input.[pos] in
      match c with
      | ' ' | '\t' | '\n' | '\r' -> scan (pos + 1) acc

      (* Comments: -- to end of line (Haskell style) *)
      | '-' when pos + 1 < len && input.[pos + 1] = '-' ->
          let rec skip_comment p =
            if p >= len || input.[p] = '\n' then p
            else skip_comment (p + 1)
          in
          scan (skip_comment (pos + 2)) acc

      (* Single character tokens *)
      | ':' -> scan (pos + 1) (TokColon :: acc)
      | ';' -> scan (pos + 1) (TokSemicolon :: acc)
      | '(' -> scan (pos + 1) (TokLParen :: acc)
      | ')' -> scan (pos + 1) (TokRParen :: acc)
      | '[' -> scan (pos + 1) (TokLBracket :: acc)
      | ']' -> scan (pos + 1) (TokRBracket :: acc)

      (* Arrow -> *)
      | '-' when pos + 1 < len && input.[pos + 1] = '>' ->
          scan (pos + 2) (TokArrow :: acc)

      (* Numbers *)
      | '0'..'9' | '-' ->
          let rec scan_number p =
            if p >= len then p
            else match input.[p] with
            | '0'..'9' -> scan_number (p + 1)
            | _ -> p
          in
          let end_pos = scan_number (pos + 1) in
          let num_str = String.sub input pos (end_pos - pos) in
          let num = Int64.of_string num_str in
          scan end_pos (TokInt num :: acc)

      (* Words (identifiers and operators) *)
      | _ when c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c = '_' ->
          let rec scan_word p =
            if p >= len then p
            else match input.[p] with
            | 'a'..'z' | 'A'..'Z' | '0'..'9' | '_' | '-' | '?' | '!' -> scan_word (p + 1)
            | _ -> p
          in
          let end_pos = scan_word (pos + 1) in
          let word = String.sub input pos (end_pos - pos) in
          scan end_pos (TokWord word :: acc)

      (* Operators as words *)
      | '+' | '*' | '/' | '<' | '>' | '=' ->
          let rec scan_op p =
            if p >= len then p
            else match input.[p] with
            | '+' | '-' | '*' | '/' | '<' | '>' | '=' | '!' -> scan_op (p + 1)
            | _ -> p
          in
          let end_pos = scan_op (pos + 1) in
          let op = String.sub input pos (end_pos - pos) in
          scan end_pos (TokWord op :: acc)

      | _ ->
          Printf.eprintf "Warning: skipping unknown character '%c' at position %d\n" c pos;
          scan (pos + 1) acc
  in
  scan 0 []
