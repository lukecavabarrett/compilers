open Core


module AST1 = struct

  module Literal = struct
    type t = UnitLiteral | IntLiteral of int | FloatLiteral of float | StringLiteral of string | CharLiteral of char | BoolLiteral of bool [@@deriving sexp,equal] ;;
    let to_string = function
      | UnitLiteral -> "()"
      | IntLiteral n -> Int.to_string n
      | FloatLiteral x -> Float.to_string x
      | StringLiteral s -> "\"" ^ s ^ "\""
      | CharLiteral c -> "'" ^ Char.to_string c ^ "'"
      | BoolLiteral b -> Bool.to_string b
    ;;


    let%expect_test _ =
      let () = print_s [%message ~l(StringLiteral "Hello, world! " : Literal.t)] in
      [%expect {| "Hello, world! " |}]
    ;;

  end



  module Matcher = struct
    type t = UniversalIgnore | UniversalBind of string | Literal of Literal.t | Tuple of t list | Constructor of string * (t option) ;;
    let rec to_string = function
      | UniversalIgnore -> "_"
      | UniversalBind s -> s
      | Literal l -> Literal.to_string l
      | Tuple t -> "("^(List.map t ~f:to_string |>  String.concat ~sep:", ")^")"
      | Constructor (s,None) -> s
      | Constructor (s,Some t) -> s ^ " "^(to_string t)
    ;;

    let%expect_test _ =
      let () = print_endline (to_string (Constructor ("Leaf",None))) in
      [%expect {| Leaf |}]
    ;;

    let%expect_test _ =
      let () = print_endline (to_string (Constructor ("Node", Some ( Tuple [UniversalBind "left";UniversalBind "left";UniversalBind "left"] )))) in
      [%expect {| Node (left, left, left) |}]
    ;;
  end

  module Expression = struct

    type t
      = Literal of Literal.t
      | Identifier of string
      | FunctionApplication of t * t
      | Sequence of t * t
      | IfThenElse of t * t * t
      | MatchWith of t *  ((Matcher.t * t) list)
      | LetIn of ( (string * (Matcher.t list)*t)    list) * t
        (* TODO: add AnonymousFunction, and MatchFunction *)
    ;;

    let rec to_string = function
      | Literal l -> Literal.to_string l
      | Identifier s -> s
      | FunctionApplication (f,x) -> (to_string f)^" ("^(to_string x)^")"
      | Sequence (e1,e2) -> (to_string e1)^"; "^(to_string e2)
      | IfThenElse (c,e1,e2) -> "if "^(to_string c)^" then "^(to_string e1)^" else "^(to_string e2)
      | MatchWith (e,mel) -> "match "^(to_string e)^" with \n" ^ (List.map mel ~f:(fun (m,e) -> "| "^(Matcher.to_string m)^" -> "^(to_string e)^"\n"   ) |> String.concat)
      | LetIn (d,e) -> "let " ^ (List.map d ~f:(fun (n,ml,e) -> n ^  (List.map ml ~f:Matcher.to_string  |> List.map ~f:(fun s -> " "^s) |> String.concat) ^ " = " ^ (to_string e) ) |> String.concat ~sep:"\nand ")  ^ "in\n" ^ (to_string e)    ;;


    let even = LetIn ([("even",[Matcher.UniversalBind "x"], MatchWith (Identifier "x",[
        (Matcher.Literal (Literal.IntLiteral 0),Literal (Literal.BoolLiteral true)) ;
        (Matcher.Literal (Literal.IntLiteral 1),Literal (Literal.BoolLiteral false));
        (Matcher.UniversalIgnore, FunctionApplication (Identifier "even",FunctionApplication (FunctionApplication   (Identifier "Int.(-)",Identifier "x"),  Literal (Literal.IntLiteral 2)))  )

      ])  )
      ],FunctionApplication (Identifier "even",Literal (Literal.IntLiteral 42)) );;


    let%expect_test _ =
      let () = print_endline (to_string even) in
      [%expect {|
        let even x = match x with
        | 0 -> true
        | 1 -> false
        | _ -> even (Int.(-) (x) (2))
        in
        even (42) |}]
    ;;

  end


end
  (*let literal_to_string*)

module Type = struct

  type t = Unit | Int | Char | Float | String | Tuple of t list | Variant of (string * (t option)) list  [@@deriving compare]

  let%expect_test _ = let () = print_endline (Core.Int.to_string 45) in [%expect {| 45 |}]

end

let rec even n =
    match n with
    | 0 -> true
    | n -> odd (n-1)
and odd n =
    match n with
    | 0 -> false
    | n -> even (n-1)
in even 42 ;;