(* March Database Loader - Load and link compiled code *)

open Sqlite3

exception LoaderError of string

(* Load a blob by CID *)
let load_blob db cid =
  let sql = "SELECT data FROM blobs WHERE cid = ?" in
  match prepare db sql with
  | stmt ->
      let _ = bind stmt 1 (Data.TEXT cid) in
      (match step stmt with
      | Rc.ROW ->
          let data = column stmt 0 in
          finalize stmt |> ignore;
          (match data with
          | Data.BLOB bytes -> Some bytes
          | _ -> None)
      | _ ->
          finalize stmt |> ignore;
          None)

(* Look up a word by name and get its CID *)
let lookup_word db name namespace =
  let sql = "SELECT def_cid, type_sig, is_primitive FROM words WHERE name = ? AND namespace = ?" in
  match prepare db sql with
  | stmt ->
      let _ = bind stmt 1 (Data.TEXT name) in
      let _ = bind stmt 2 (Data.TEXT namespace) in
      (match step stmt with
      | Rc.ROW ->
          let cid = column stmt 0 in
          let type_sig = column stmt 1 in
          let is_prim = column stmt 2 in
          finalize stmt |> ignore;
          (match (cid, is_prim) with
          | (Data.TEXT c, Data.INT p) ->
              let ts = match type_sig with Data.TEXT s -> Some s | _ -> None in
              Some (c, ts, p <> 0L)
          | _ -> None)
      | _ ->
          finalize stmt |> ignore;
          None)

(* Convert byte string to int64 array (cells) *)
let bytes_to_cells bytes =
  let len = String.length bytes in
  if len mod 8 <> 0 then
    raise (LoaderError (Printf.sprintf "Blob length %d is not a multiple of 8" len))
  else
    let num_cells = len / 8 in
    let cells = Array.make num_cells 0L in
    for i = 0 to num_cells - 1 do
      let offset = i * 8 in
      (* Read little-endian 64-bit int *)
      let bytes_arr = Array.init 8 (fun j ->
        Int64.of_int (Char.code bytes.[offset + j])
      ) in
      let cell = ref 0L in
      for j = 0 to 7 do
        cell := Int64.logor !cell
          (Int64.shift_left bytes_arr.(j) (j * 8))
      done;
      cells.(i) <- !cell
    done;
    cells

(* Load a word's cells from database *)
let load_word_cells db name namespace =
  match lookup_word db name namespace with
  | None -> None
  | Some (cid, _type_sig, is_primitive) ->
      if is_primitive then
        raise (LoaderError (Printf.sprintf "Cannot load primitive word '%s' as cells" name))
      else
        match load_blob db cid with
        | None -> raise (LoaderError (Printf.sprintf "Blob not found for CID %s" cid))
        | Some bytes ->
            let cells = bytes_to_cells bytes in
            Some cells

(* Print cells for debugging *)
let print_cells cells =
  let rec print_from i =
    if i >= Array.length cells then ()
    else
      let cell = cells.(i) in
      let tag = Int64.to_int (Int64.logand cell 0x3L) in
      match tag with
      | 0 -> (* XT *)
          if cell = 0L then
            Printf.printf "  [%d] EXIT: 0x%Lx\n" i cell
          else
            Printf.printf "  [%d] XT: 0x%Lx\n" i cell;
          print_from (i + 1)
      | 1 -> (* LIT *)
          let value = Int64.shift_right cell 2 in
          Printf.printf "  [%d] LIT: 0x%Lx (value=%Ld)\n" i cell value;
          print_from (i + 1)
      | 2 -> (* LST or LNT *)
          let bit2 = Int64.to_int (Int64.logand (Int64.shift_right_logical cell 2) 1L) in
          if bit2 = 1 then begin
            (* LNT - next N cells are raw literals *)
            let count = Int64.to_int (Int64.shift_right_logical cell 3) in
            Printf.printf "  [%d] LNT: count=%d\n" i count;
            for j = 1 to count do
              if i + j < Array.length cells then
                Printf.printf "  [%d]   raw: %Ld\n" (i + j) cells.(i + j)
            done;
            print_from (i + count + 1)
          end else begin
            (* LST *)
            let sym_id = Int64.shift_right_logical cell 2 in
            Printf.printf "  [%d] LST: 0x%Lx (sym_id=%Ld)\n" i cell sym_id;
            print_from (i + 1)
          end
      | 3 -> (* EXT *)
          Printf.printf "  [%d] EXT: 0x%Lx\n" i cell;
          print_from (i + 1)
      | _ ->
          Printf.printf "  [%d] ???: 0x%Lx\n" i cell;
          print_from (i + 1)
  in
  print_from 0
