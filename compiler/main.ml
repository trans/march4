(* March Bootstrap Compiler - Main Entry Point *)

open Types
open Lexer
open Parser
open Codegen
open Database

let usage () =
  Printf.eprintf "Usage: marchc [options] <source-file>\n";
  Printf.eprintf "Options:\n";
  Printf.eprintf "  -o <db>     Output database file (default: march.db)\n";
  Printf.eprintf "  -v          Verbose output\n";
  Printf.eprintf "  --tokens    Show tokens and exit\n";
  Printf.eprintf "  --ast       Show AST and exit\n";
  exit 1

let main () =
  let source_file = ref None in
  let db_file = ref "../march.db" in
  let schema_file = ref "../schema.sql" in
  let verbose = ref false in
  let show_tokens = ref false in
  let show_ast = ref false in

  (* Parse command line arguments *)
  let rec parse_args = function
    | [] -> ()
    | "-o" :: file :: rest ->
        db_file := file;
        parse_args rest
    | "-v" :: rest ->
        verbose := true;
        parse_args rest
    | "--tokens" :: rest ->
        show_tokens := true;
        parse_args rest
    | "--ast" :: rest ->
        show_ast := true;
        parse_args rest
    | file :: rest ->
        source_file := Some file;
        parse_args rest
  in
  parse_args (List.tl (Array.to_list Sys.argv));

  let source = match !source_file with
    | None -> usage ()
    | Some f -> f
  in

  try
    (* Read source file *)
    let ic = open_in source in
    let content = really_input_string ic (in_channel_length ic) in
    close_in ic;

    if !verbose then
      Printf.printf "Compiling %s...\n" source;

    (* Lexical analysis *)
    let tokens = tokenize content in
    if !show_tokens then begin
      Printf.printf "=== Tokens ===\n";
      List.iter (fun tok -> Printf.printf "%s\n" (string_of_token tok)) tokens;
      exit 0
    end;

    (* Parsing *)
    let program = parse tokens in
    if !show_ast then begin
      Printf.printf "=== AST ===\n";
      List.iter (fun word ->
        Printf.printf "Word: %s\n" word.name;
        Printf.printf "  Body: %d expressions\n" (List.length word.body)
      ) program.words;
      exit 0
    end;

    if !verbose then
      Printf.printf "Parsed %d words\n" (List.length program.words);

    (* Code generation *)
    let program_data = generate program in

    if !verbose then begin
      Printf.printf "Generated code:\n";
      List.iter (fun (name, cid, bytes, cells) ->
        Printf.printf "  %s: %s (%d cells, %d bytes)\n"
          name cid (List.length cells) (String.length bytes)
      ) program_data
    end;

    (* Store in database *)
    let db = open_db !db_file in

    (* Initialize database if needed *)
    if not (Sys.file_exists !db_file) || (Unix.stat !db_file).Unix.st_size = 0 then begin
      if !verbose then
        Printf.printf "Initializing database...\n";
      init_db db !schema_file
    end;

    (* Store compiled program *)
    store_program db program_data "user";

    Printf.printf "Successfully compiled %s\n" source;
    Printf.printf "Output: %s\n" !db_file

  with
  | Sys_error msg ->
      Printf.eprintf "Error: %s\n" msg;
      exit 1
  | ParseError msg ->
      Printf.eprintf "Parse error: %s\n" msg;
      exit 1
  | DatabaseError msg ->
      Printf.eprintf "Database error: %s\n" msg;
      exit 1

let () = main ()
