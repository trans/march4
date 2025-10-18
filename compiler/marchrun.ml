(* March Runner - Load and execute compiled words *)

open Database
open Loader
open Vm_lib.Vm_ffi

let usage () =
  Printf.eprintf "Usage: marchrun <database> <word-name> [namespace]\n";
  Printf.eprintf "\n";
  Printf.eprintf "Load a compiled word from the database and execute it.\n";
  Printf.eprintf "Displays the resulting data stack.\n";
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

    (* Load cells *)
    match load_word_cells db word_name namespace with
    | None ->
        Printf.eprintf "Error: Word '%s' not found in namespace '%s'\n" word_name namespace;
        exit 1
    | Some cells ->
        Printf.printf "Loaded %d cells\n" (Array.length cells);

        (* Show loaded code *)
        Printf.printf "\nCode:\n";
        print_cells cells;

        (* Execute *)
        Printf.printf "\nExecuting...\n";
        let stack = execute cells in

        (* Display results *)
        let depth = Array.length stack in
        if depth = 0 then begin
          Printf.printf "\nResult: empty stack\n"
        end else begin
          Printf.printf "\nResult stack (%d item%s):\n" depth (if depth = 1 then "" else "s");
          (* Print stack from top to bottom *)
          for i = 0 to depth - 1 do
            Printf.printf "  [%d] = %Ld\n" i stack.(i)
          done
        end;

        Printf.printf "\nExecution successful!\n"

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
  | Failure msg ->
      Printf.eprintf "VM error: %s\n" msg;
      exit 1

let () = main ()
