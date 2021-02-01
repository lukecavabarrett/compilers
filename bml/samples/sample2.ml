
(* IDEA2 *)

type alive_file = | Fd of file;;
type 'm acf_inner = | Log_fun of ('m -> 'm acf_inner) * alive_file;;
type msg = | Int of int | Str of string ;;
let rec make_logger alive_file f = Log_fun ((fun x -> f  alive_file x; make_logger alive_file f), alive_file) ;;
let print_with_time (Fd file) msg =
    fprint_time file (now ());
    fprint_str file " | INFO: (";
     (match msg with
        | Str s -> fprint_str file s
        | Int n -> fprint_int file n
        );
    fprint_str file ")\n" ;;
let acf_open path mode = make_logger ((Fd (fopen path mode)) ~> (fun (Fd fd) -> print_with_time (Fd fd) (Str "CLOSING FILE"); fclose fd)) print_with_time;;
let log x (Log_fun (f,af)) = f x;;

let rec slow_fib n = if n<2 then n else (slow_fib (n-1)) + (slow_fib (n-2));;

let rec play_fib logger =
    let logger = logger |> log (Str "Waiting for user input") in
    let x = scan_int () in
    if x<0 then () else
    let logger = logger |> log (Str "User entered n = ") |> log (Int x) |> log (Str "Computing fibonacci of n...")in
    let y = slow_fib x in
    let () = println_int y in
    let logger = logger |> log (Str "Computed!") |> log (Int y) in
    play_fib logger;;


acf_open "log.txt" "w+" |> log (Str "Hello") |> log (Str "This is logging some information") |> play_fib;;
let tee f x = f x ; x ;;
(*
This would play two games
acf_open "log.txt" "w+" |> log (Str "Hello") |> log (Str "This is logging some information") |> tee play_fib |> play_fib;;
*)