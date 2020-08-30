let map t f =
match t with
| Some value -> Some (f value)
| None -> None

let next_event,server,conn,error,ok = () in
match next_event server conn with
| Error _ , t ->  t
| Ok (Error _), t -> error t
| OK (Ok (a,b)), _ -> ok a
| Nothing 43 , _ ->  conn ;;

let int_add x y = () ;;

let rec start x =
    (x , def_start (int_add x 1))
and def_start x () =
    start x
in
    start 45 ;;

let rec multiply_out e =
    match e with
    | Times (e1, Plus (e2, e3)) ->
       Plus (Times (multiply_out e1, multiply_out e2),
             Times (multiply_out e1, multiply_out e3))
    | Times (Plus (e1, e2), e3) ->
       Plus (Times (multiply_out e1, multiply_out e3),
             Times (multiply_out e2, multiply_out e3))
    | Plus (left, right) ->
       Plus (multiply_out left, multiply_out right)
    | Minus (left, right) ->
       Minus (multiply_out left, multiply_out right)
    | Times (left, right) ->
       Times (multiply_out left, multiply_out right)
    | Divide (left, right) ->
       Divide (multiply_out left, multiply_out right)
    | Value v -> Value v
in

match result with
| true -> false
| false -> true ;;

