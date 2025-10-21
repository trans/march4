(* March Type Checker - Static Type Checking with Overload Resolution *)

open Types

exception TypeError of string

(* Type environment - tracks available words and their signatures *)
type type_env = {
  words: (string, word_signature list) Hashtbl.t;  (* word name -> list of signatures *)
  debug_mode: bool;  (* Enable debug type tracking *)
}

(* Word signature for overload resolution *)
and word_signature = {
  sig_name: string;
  sig_inputs: march_type list;
  sig_outputs: march_type list;
  sig_cid: string option;  (* CID if already compiled *)
  sig_priority: int;  (* Explicit priority for tie-breaking *)
}

(* Shadow type stack state during compilation *)
type type_state = {
  stack: march_type list;  (* Type stack (top is head) *)
  env: type_env;
}

(* Create initial environment *)
let create_env debug_mode =
  let env = {
    words = Hashtbl.create 100;
    debug_mode;
  } in

  (* Register primitive operations *)
  let add_primitive name inputs outputs =
    let sig_entry = {
      sig_name = name;
      sig_inputs = inputs;
      sig_outputs = outputs;
      sig_cid = None;
      sig_priority = 0;
    } in
    let existing =
      if Hashtbl.mem env.words name then
        Hashtbl.find env.words name
      else []
    in
    Hashtbl.replace env.words name (sig_entry :: existing)
  in

  (* Stack operations *)
  add_primitive "dup" [TInt64] [TInt64; TInt64];
  add_primitive "drop" [TInt64] [];
  add_primitive "swap" [TInt64; TInt64] [TInt64; TInt64];
  add_primitive "over" [TInt64; TInt64] [TInt64; TInt64; TInt64];
  add_primitive "rot" [TInt64; TInt64; TInt64] [TInt64; TInt64; TInt64];

  (* Arithmetic *)
  add_primitive "+" [TInt64; TInt64] [TInt64];
  add_primitive "-" [TInt64; TInt64] [TInt64];
  add_primitive "*" [TInt64; TInt64] [TInt64];
  add_primitive "/" [TInt64; TInt64] [TInt64];
  add_primitive "mod" [TInt64; TInt64] [TInt64];

  (* Comparisons *)
  add_primitive "=" [TInt64; TInt64] [TBool];
  add_primitive "<>" [TInt64; TInt64] [TBool];
  add_primitive "<" [TInt64; TInt64] [TBool];
  add_primitive ">" [TInt64; TInt64] [TBool];
  add_primitive "<=" [TInt64; TInt64] [TBool];
  add_primitive ">=" [TInt64; TInt64] [TBool];

  (* Bitwise *)
  add_primitive "and" [TInt64; TInt64] [TInt64];
  add_primitive "or" [TInt64; TInt64] [TInt64];
  add_primitive "xor" [TInt64; TInt64] [TInt64];
  add_primitive "not" [TInt64] [TInt64];

  env

(* Compute specificity score for type matching *)
(* Higher score = more specific match *)
let rec type_specificity ty1 ty2 =
  match (ty1, ty2) with
  | (t1, t2) when t1 = t2 -> 100  (* Exact match *)
  | (_, TUnknown) -> 10  (* Matches unknown (polymorphic) *)
  | (TInt64, TVar _) -> 50  (* Concrete matches type var *)
  | (TBool, TVar _) -> 50
  | (TPtr t1, TPtr t2) -> type_specificity t1 t2
  | (TFunc (ins1, outs1), TFunc (ins2, outs2)) ->
      let in_score = List.fold_left2 (fun acc t1 t2 ->
        acc + type_specificity t1 t2
      ) 0 ins1 ins2 in
      let out_score = List.fold_left2 (fun acc t1 t2 ->
        acc + type_specificity t1 t2
      ) 0 outs1 outs2 in
      in_score + out_score
  | _ -> 0  (* No match *)

(* Check if signature matches current stack state *)
let signature_matches sig_entry stack =
  (* Check if we have enough values on stack *)
  if List.length stack < List.length sig_entry.sig_inputs then
    None
  else
    (* Get top N values from stack (where N = number of inputs) *)
    let stack_top = List.rev (List.filteri (fun i _ ->
      i < List.length sig_entry.sig_inputs
    ) (List.rev stack)) in

    (* Check if types match (with specificity scoring) *)
    let scores = List.map2 type_specificity stack_top sig_entry.sig_inputs in
    if List.for_all (fun score -> score > 0) scores then
      let total_score = List.fold_left (+) 0 scores in
      Some total_score
    else
      None

(* Resolve overloaded word - pick best match based on specificity *)
let resolve_overload word_name signatures stack =
  (* Find all matching signatures with their scores *)
  let matches = List.filter_map (fun sig_entry ->
    match signature_matches sig_entry stack with
    | Some score -> Some (sig_entry, score)
    | None -> None
  ) signatures in

  match matches with
  | [] ->
      let stack_str = String.concat " " (List.map string_of_type stack) in
      raise (TypeError (Printf.sprintf
        "No matching signature for '%s' with stack: %s" word_name stack_str))

  | [(sig_entry, _)] ->
      sig_entry  (* Unique match *)

  | _ ->
      (* Multiple matches - pick most specific *)
      let sorted = List.sort (fun (sig1, score1) (sig2, score2) ->
        if score1 <> score2 then
          compare score2 score1  (* Higher score first *)
        else
          (* Tie-breaker: use priority *)
          compare sig2.sig_priority sig1.sig_priority
      ) matches in

      let (best_sig, best_score) = List.hd sorted in
      let (second_sig, second_score) = List.nth sorted 1 in

      (* Check for ambiguity *)
      if best_score = second_score && best_sig.sig_priority = second_sig.sig_priority then
        raise (TypeError (Printf.sprintf
          "Ambiguous overload for '%s' - multiple signatures match equally" word_name))
      else
        best_sig

(* Apply word signature to type stack *)
let apply_signature sig_entry stack =
  (* Remove inputs from stack *)
  let stack' = List.filteri (fun i _ ->
    i >= List.length sig_entry.sig_inputs
  ) (List.rev stack) |> List.rev in

  (* Add outputs to stack *)
  List.rev_append (List.rev sig_entry.sig_outputs) stack'

(* Type check a single expression *)
let rec check_expr state expr =
  match expr with
  | ELit _ ->
      (* Literal pushes an int64 *)
      { state with stack = TInt64 :: state.stack }

  | EWord name ->
      (* Look up word and resolve overload *)
      if not (Hashtbl.mem state.env.words name) then
        raise (TypeError (Printf.sprintf "Unknown word: %s" name));

      let signatures = Hashtbl.find state.env.words name in
      let sig_entry = resolve_overload name signatures state.stack in

      (* Apply signature to stack *)
      let new_stack = apply_signature sig_entry state.stack in
      { state with stack = new_stack }

  | ECall (name, args) ->
      (* Type check arguments first *)
      let state' = List.fold_left check_expr state args in

      (* Then check the call *)
      if not (Hashtbl.mem state'.env.words name) then
        raise (TypeError (Printf.sprintf "Unknown word: %s" name));

      let signatures = Hashtbl.find state'.env.words name in
      let sig_entry = resolve_overload name signatures state'.stack in
      let new_stack = apply_signature sig_entry state'.stack in
      { state with stack = new_stack }

  | ESeq exprs ->
      List.fold_left check_expr state exprs

(* Type check a word definition *)
let check_word_def env word =
  (* Create initial state with declared outputs *)
  let initial_state = {
    stack = [];  (* Start with empty stack *)
    env;
  } in

  (* Type check body *)
  let final_state = List.fold_left check_expr initial_state word.body in

  (* Check that final stack matches declared return types *)
  if word.returns <> [] then begin
    (* Verify stack has expected types *)
    let stack_len = List.length final_state.stack in
    let returns_len = List.length word.returns in

    if stack_len < returns_len then
      raise (TypeError (Printf.sprintf
        "Word '%s': Expected %d return values, but stack has %d"
        word.name returns_len stack_len));

    (* Check types match (top of stack should match returns) *)
    let stack_top = List.rev (List.filteri (fun i _ -> i < returns_len)
      (List.rev final_state.stack)) in

    List.iter2 (fun stack_ty return_ty ->
      if type_specificity stack_ty return_ty = 0 then
        raise (TypeError (Printf.sprintf
          "Word '%s': Type mismatch - expected %s, got %s"
          word.name (string_of_type return_ty) (string_of_type stack_ty)))
    ) stack_top word.returns
  end;

  (* Return inferred signature *)
  {
    sig_name = word.name;
    sig_inputs = [];  (* TODO: infer from usage *)
    sig_outputs = if word.returns = [] then final_state.stack else word.returns;
    sig_cid = None;
    sig_priority = 0;
  }

(* Type check entire program *)
let check_program env (prog : program) =
  (* Check each word definition *)
  let signatures = List.map (fun word ->
    try
      let sig_entry = check_word_def env word in
      (* Register word in environment for subsequent words *)
      let existing =
        if Hashtbl.mem env.words word.name then
          Hashtbl.find env.words word.name
        else []
      in
      Hashtbl.replace env.words word.name (sig_entry :: existing);
      (word.name, Some sig_entry)
    with TypeError msg ->
      Printf.eprintf "Type error in word '%s': %s\n" word.name msg;
      (word.name, None)
  ) prog.words in

  (* Return list of successfully type-checked words *)
  List.filter_map (fun (name, sig_opt) ->
    match sig_opt with
    | Some sig_entry -> Some (name, sig_entry)
    | None -> None
  ) signatures

(* Generate debug instrumentation for type tracking *)
let instrument_for_debug sig_entry =
  (* Generate debug assertions to check types at runtime *)
  (* This would emit extra cells that validate stack types *)
  (* For now, just return a comment *)
  Printf.sprintf "DEBUG: %s : %s -> %s"
    sig_entry.sig_name
    (String.concat " " (List.map string_of_type sig_entry.sig_inputs))
    (String.concat " " (List.map string_of_type sig_entry.sig_outputs))
