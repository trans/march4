(* March Bootstrap Compiler - Main Entry Point *)

open Types
open Lexer
open Parser
open Typecheck
open Codegen
open Database

let usage () =
  Printf.eprintf "Usage: marchc [options] <source-file>\n";
  Printf.eprintf "Options:\n";
  Printf.eprintf "  -o <db>     Output database file (default: march.db)\n";
  Printf.eprintf "  -v          Verbose output\n";
  Printf.eprintf "  -g          Debug mode (with type tracking)\n";
  Printf.eprintf "  --tokens    Show tokens and exit\n";
  Printf.eprintf "  --ast       Show AST and exit\n";
  Printf.eprintf "  --types     Show type signatures and exit\n";
  exit 1

let main () =
  let source_file = ref None in
  let db_file = ref "../march.db" in
  let schema_file = ref "../schema.sql" in
  let verbose = ref false in
  let debug_mode = ref false in
  let show_tokens = ref false in
  let show_ast = ref false in
  let show_types = ref false in

  (* Parse command line arguments *)
  let rec parse_args = function
    | [] -> ()
    | "-o" :: file :: rest ->
        db_file := file;
        parse_args rest
    | "-v" :: rest ->
        verbose := true;
        parse_args rest
    | "-g" :: rest ->
        debug_mode := true;
        parse_args rest
    | "--tokens" :: rest ->
        show_tokens := true;
        parse_args rest
    | "--ast" :: rest ->
        show_ast := true;
        parse_args rest
    | "--types" :: rest ->
        show_types := true;
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

    (* Type checking *)
    let type_env = create_env !debug_mode in
    let type_sigs = check_program type_env program in

    if !show_types then begin
      Printf.printf "=== Type Signatures ===\n";
      List.iter (fun (name, sig_entry) ->
        Printf.printf "%s : %s -> %s\n"
          name
          (String.concat " " (List.map string_of_type sig_entry.sig_inputs))
          (String.concat " " (List.map string_of_type sig_entry.sig_outputs))
      ) type_sigs;
      exit 0
    end;

    if !verbose then
      Printf.printf "Type checked %d words\n" (List.length type_sigs);

    if !debug_mode then
      Printf.printf "Debug mode enabled - runtime type tracking active\n";

    (* Code generation *)
    let program_data = generate program in

    if !verbose then begin
      Printf.printf "Generated code:\n";
      List.iter (fun (name, cid, bytes, cells) ->
        Printf.printf "  %s: %s (%d cells, %d bytes)\n"
          name cid (List.length cells) (String.length bytes);
        if !debug_mode then begin
          Printf.printf "    Raw cells: %s\n"
            (String.concat " " (List.map (Printf.sprintf "%Ld") cells));
          Printf.printf "    Decoded: ";
          let rec print_cells = function
            | [] -> ()
            | cell :: rest ->
                let tag = Types.decode_tag cell in
                match tag with
                | Types.TagLNT ->
                    let count = Types.decode_lnt cell in
                    Printf.printf "[LNT:%Ld: " count;
                    let rec print_n_lits n remaining =
                      if n <= 0L then remaining
                      else match remaining with
                        | lit :: rest' ->
                            Printf.printf "%Ld " lit;
                            print_n_lits (Int64.sub n 1L) rest'
                        | [] -> []
                    in
                    let rest' = print_n_lits count rest in
                    Printf.printf "] ";
                    print_cells rest'
                | Types.TagXT when cell = 0L ->
                    Printf.printf "[EXIT] ";
                    print_cells rest
                | Types.TagXT ->
                    Printf.printf "[XT:%Ld] " (Types.decode_xt cell);
                    print_cells rest
                | Types.TagLIT ->
                    Printf.printf "[LIT:%Ld] " (Types.decode_lit cell);
                    print_cells rest
                | Types.TagLST ->
                    Printf.printf "[LST:%Ld] " (Types.decode_lst cell);
                    print_cells rest
                | Types.TagEXT ->
                    Printf.printf "[EXT:%Ld] " (Types.decode_ext cell);
                    print_cells rest
          in
          print_cells cells;
          Printf.printf "\n"
        end
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
