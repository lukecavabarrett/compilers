(*
To compile to native code: ocamlopt sort.ml -o c_sort
To compile to bytecode:
        ocamlc sort.ml -o b_sort
        ocamlrun b_sort
To compile with mine:
    ./mlc sort.ml -o sort
*)
type 'a list = | Nil | Cons of 'a * 'a list;;

let rec length_ l acc = match l with | Nil -> acc | Cons(_,l) -> length_ l (acc+1)
and length l = length_ l 0;;

let rec list_range_ i  acc = if i<0 then acc else list_range_ (i-1)  (Cons(i,acc))
and list_range n = list_range_ (n-1) Nil;;

let rec reverse_ l acc = match l with | Nil -> acc | Cons(hd,tl) -> reverse_ tl (Cons(hd,acc))
and reverse l = reverse_ l Nil;;

let rec merge_ a b racc =
    match a with
    | Nil -> reverse_ racc b (* use b as tail *)
    | Cons (a_hd,a_tl) -> match b with
                     | Nil -> reverse_ racc a (* use b as tail *)
                     | Cons (b_hd,b_tl) ->
                        if b_hd < a_hd then  merge_ a b_tl (Cons(b_hd,racc)) else merge_ b a_tl (Cons(a_hd,racc));;
let merge a b = merge_ a b Nil;;


let rec split_ l (acc0,acc1) =
    match l with
    | Nil -> (acc0,acc1)
    | Cons (hd,tl) -> split_ tl (Cons(hd,acc1),acc0)
and split l = split_ l (Nil,Nil);;

let rec sort l =
    match l with
    | Nil -> Nil
    | Cons(_,Nil) -> l
    | _ -> let a,b = split l in
           let a = sort a
           and b = sort b
           in merge a b ;;

let rec print_comma_ l = 
	match l with
	| Nil -> print_string "]\n"
	| Cons(x,xs) -> print_string ", "; print_int x; print_comma_ xs;;
let println_list l = 
	match l with
	| Nil -> print_string "[]\n"
	| Cons(x,xs) -> print_string "["; print_int x; print_comma_ xs;;
	 		     

let tee f x = f x; x ;;

let n = 10;;
list_range n |> reverse |> sort |> println_list;;
(* list_range n |> tee println_list |> reverse |> tee println_list |> sort |> println_list;; *)

