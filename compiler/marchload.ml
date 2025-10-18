(* March Loader Test Tool - Load and display compiled words *)

open Database
open Loader

let usage () =
  Printf.eprintf "Usage: marchload <database> <word-name> [namespace]\n";
  exit 1

let main () =
  if Array.length Sys.argv < 3 then
    usage ();

  let db_file = Sys.argv.(1) in
  let word_name = Sys.argv.(2) in
  let namespace = if Array.length Sys.argv >= 4 then Sys.argv.(3) else "user" in

  try
    (* Open database *)
    let db = open_db db_file in

    Printf.printf "Loading word '%s' from namespace '%s'...\n" word_name namespace;

    (* Look up word *)
    match lookup_word db word_name namespace with
    | None ->
        Printf.eprintf "Error: Word '%s' not found in namespace '%s'\n" word_name namespace;
        exit 1
    | Some (cid, type_sig, is_primitive) ->
        Printf.printf "Found word:\n";
        Printf.printf "  CID: %s\n" cid;
        Printf.printf "  Type: %s\n" (match type_sig with Some s -> s | None -> "(unknown)");
        Printf.printf "  Primitive: %s\n" (if is_primitive then "yes" else "no");

        if is_primitive then begin
          Printf.printf "\nCannot load primitive words as cells.\n";
          exit 0
        end;

        (* Load cells *)
        match load_word_cells db word_name namespace with
        | None ->
            Printf.eprintf "Error: Failed to load word cells\n";
            exit 1
        | Some cells ->
            Printf.printf "\nLoaded %d cells:\n" (Array.length cells);
            print_cells cells;

            Printf.printf "\nLoad successful!\n"

  with
  | DatabaseError msg ->
      Printf.eprintf "Database error: %s\n" msg;
      exit 1
  | LoaderError msg ->
      Printf.eprintf "Loader error: %s\n" msg;
      exit 1
  | Sys_error msg ->
      Printf.eprintf "System error: %s\n" msg;
      exit 1

let () = main ()
