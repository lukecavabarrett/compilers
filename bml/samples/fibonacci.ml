let rec fibonacci n = if n<2 then n else (fibonacci (n-1)) + (fibonacci (n-2));;

(* ITER *)
let rec iter_ f i n = if i=n then () else f i; iter_ f (i+1) n;;
let iter f n = iter_ f 0 n;;

let rec factorial n = if n=0 then 1 else n*(factorial (n-1));;

let rec main () =
        let input = scan_int () in
        if input<0 then ()
        else fibonacci input |> println_int; main () ;;

main ();;
