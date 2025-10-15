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
