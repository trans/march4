(* March Database Interface - SQLite Integration *)

open Sqlite3

exception DatabaseError of string

(* Open or create database *)
let open_db path =
  match db_open path with
  | db -> db

(* Initialize database with schema if needed *)
let init_db db schema_file =
  try
    let ic = open_in schema_file in
    let schema = really_input_string ic (in_channel_length ic) in
    close_in ic;

    (* Execute schema *)
    match exec db schema with
    | Rc.OK -> ()
    | rc -> raise (DatabaseError (Printf.sprintf "Failed to initialize database: %s" (Rc.to_string rc)))
  with
  | Sys_error msg -> raise (DatabaseError msg)

(* Store a blob in the database *)
let store_blob db cid kind flags bytes =
  let sql = "INSERT OR IGNORE INTO blobs (cid, kind, flags, len, data) VALUES (?, ?, ?, ?, ?)" in
  match prepare db sql with
  | stmt ->
      let len = String.length bytes in
      let _ = bind stmt 1 (Data.TEXT cid) in
      let _ = bind stmt 2 (Data.INT (Int64.of_int kind)) in
      let _ = bind stmt 3 (Data.INT (Int64.of_int flags)) in
      let _ = bind stmt 4 (Data.INT (Int64.of_int len)) in
      let _ = bind stmt 5 (Data.BLOB bytes) in

      (match step stmt with
      | Rc.DONE -> finalize stmt |> ignore
      | rc ->
          finalize stmt |> ignore;
          raise (DatabaseError (Printf.sprintf "Failed to store blob: %s" (Rc.to_string rc))))

(* Store a word definition *)
let store_word db name namespace cid type_sig is_primitive arch doc =
  let sql =
    "INSERT INTO words (name, namespace, def_cid, type_sig, is_primitive, architecture, doc) \
     VALUES (?, ?, ?, ?, ?, ?, ?)"
  in
  match prepare db sql with
  | stmt ->
      let _ = bind stmt 1 (Data.TEXT name) in
      let _ = bind stmt 2 (Data.TEXT namespace) in
      let _ = bind stmt 3 (Data.TEXT cid) in
      let _ = bind stmt 4 (match type_sig with Some s -> Data.TEXT s | None -> Data.NULL) in
      let _ = bind stmt 5 (Data.INT (if is_primitive then 1L else 0L)) in
      let _ = bind stmt 6 (match arch with Some a -> Data.TEXT a | None -> Data.NULL) in
      let _ = bind stmt 7 (match doc with Some d -> Data.TEXT d | None -> Data.NULL) in

      (match step stmt with
      | Rc.DONE -> finalize stmt |> ignore
      | rc ->
          finalize stmt |> ignore;
          raise (DatabaseError (Printf.sprintf "Failed to store word: %s" (Rc.to_string rc))))

(* Store compiled program *)
let store_program db program_data namespace =
  List.iter (fun (name, cid, bytes, _cells) ->
    (* Store the blob *)
    store_blob db cid 0 0 bytes;

    (* Store the word *)
    store_word db name namespace cid None false None None;

    Printf.printf "Stored word '%s' with CID %s (%d bytes)\n" name cid (String.length bytes)
  ) program_data

(* ============================================================================ *)
(* Symbol Table Operations *)
(* ============================================================================ *)

(* Get or insert a symbol, returning its ID *)
let get_or_insert_symbol db name =
  (* First try to look up existing symbol *)
  let lookup_sql = "SELECT id FROM symbols WHERE name = ?" in
  match prepare db lookup_sql with
  | lookup_stmt ->
      let _ = bind lookup_stmt 1 (Data.TEXT name) in
      (match step lookup_stmt with
      | Rc.ROW ->
          (* Symbol exists, return its ID *)
          let id = column lookup_stmt 0 in
          finalize lookup_stmt |> ignore;
          (match id with
          | Data.INT i -> i
          | _ -> raise (DatabaseError "Symbol ID is not an integer"))
      | _ ->
          (* Symbol doesn't exist, insert it *)
          finalize lookup_stmt |> ignore;
          let insert_sql = "INSERT INTO symbols (name) VALUES (?)" in
          (match prepare db insert_sql with
          | insert_stmt ->
              let _ = bind insert_stmt 1 (Data.TEXT name) in
              (match step insert_stmt with
              | Rc.DONE ->
                  finalize insert_stmt |> ignore;
                  last_insert_rowid db
              | rc ->
                  finalize insert_stmt |> ignore;
                  raise (DatabaseError (Printf.sprintf "Failed to insert symbol: %s" (Rc.to_string rc))))))

(* Look up symbol name by ID *)
let get_symbol_name db sym_id =
  let sql = "SELECT name FROM symbols WHERE id = ?" in
  match prepare db sql with
  | stmt ->
      let _ = bind stmt 1 (Data.INT sym_id) in
      (match step stmt with
      | Rc.ROW ->
          let name = column stmt 0 in
          finalize stmt |> ignore;
          (match name with
          | Data.TEXT s -> Some s
          | _ -> None)
      | _ ->
          finalize stmt |> ignore;
          None)

(* Get all symbols (for debugging) *)
let get_all_symbols db =
  let sql = "SELECT id, name FROM symbols ORDER BY id" in
  match prepare db sql with
  | stmt ->
      let rec collect acc =
        match step stmt with
        | Rc.ROW ->
            let id = column stmt 0 in
            let name = column stmt 1 in
            (match (id, name) with
            | (Data.INT i, Data.TEXT n) -> collect ((i, n) :: acc)
            | _ -> collect acc)
        | _ -> List.rev acc
      in
      let symbols = collect [] in
      finalize stmt |> ignore;
      symbols
