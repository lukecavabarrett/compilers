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
(* EXAMPLE result in log.txt:

Mon Feb  1 00:55:54 2021
 | INFO: (Hello)
Mon Feb  1 00:55:54 2021
 | INFO: (This is logging some information)
Mon Feb  1 00:55:54 2021
 | INFO: (Waiting for user input)
Mon Feb  1 00:55:57 2021
 | INFO: (User entered n = )
Mon Feb  1 00:55:57 2021
 | INFO: (2 )
Mon Feb  1 00:55:57 2021
 | INFO: (Computing fibonacci of n...)
Mon Feb  1 00:55:57 2021
 | INFO: (Computed!)
Mon Feb  1 00:55:57 2021
 | INFO: (1 )
Mon Feb  1 00:55:57 2021
 | INFO: (Waiting for user input)
Mon Feb  1 00:56:00 2021
 | INFO: (User entered n = )
Mon Feb  1 00:56:00 2021
 | INFO: (3 )
Mon Feb  1 00:56:00 2021
 | INFO: (Computing fibonacci of n...)
Mon Feb  1 00:56:00 2021
 | INFO: (Computed!)
Mon Feb  1 00:56:00 2021
 | INFO: (2 )
Mon Feb  1 00:56:00 2021
 | INFO: (Waiting for user input)
Mon Feb  1 00:56:02 2021
 | INFO: (User entered n = )
Mon Feb  1 00:56:02 2021
 | INFO: (4 )
Mon Feb  1 00:56:02 2021
 | INFO: (Computing fibonacci of n...)
Mon Feb  1 00:56:02 2021
 | INFO: (Computed!)
Mon Feb  1 00:56:02 2021
 | INFO: (3 )
Mon Feb  1 00:56:02 2021
 | INFO: (Waiting for user input)
Mon Feb  1 00:56:03 2021
 | INFO: (User entered n = )
Mon Feb  1 00:56:03 2021
 | INFO: (5 )
Mon Feb  1 00:56:03 2021
 | INFO: (Computing fibonacci of n...)
Mon Feb  1 00:56:03 2021
 | INFO: (Computed!)
Mon Feb  1 00:56:03 2021
 | INFO: (5 )
Mon Feb  1 00:56:03 2021
 | INFO: (Waiting for user input)
Mon Feb  1 00:56:04 2021
 | INFO: (User entered n = )
Mon Feb  1 00:56:04 2021
 | INFO: (6 )
Mon Feb  1 00:56:04 2021
 | INFO: (Computing fibonacci of n...)
Mon Feb  1 00:56:04 2021
 | INFO: (Computed!)
Mon Feb  1 00:56:04 2021
 | INFO: (8 )
Mon Feb  1 00:56:04 2021
 | INFO: (Waiting for user input)
Mon Feb  1 00:56:04 2021
 | INFO: (User entered n = )
Mon Feb  1 00:56:04 2021
 | INFO: (7 )
Mon Feb  1 00:56:04 2021
 | INFO: (Computing fibonacci of n...)
Mon Feb  1 00:56:04 2021
 | INFO: (Computed!)
Mon Feb  1 00:56:04 2021
 | INFO: (13 )
Mon Feb  1 00:56:04 2021
 | INFO: (Waiting for user input)
Mon Feb  1 00:56:05 2021
 | INFO: (User entered n = )
Mon Feb  1 00:56:05 2021
 | INFO: (8 )
Mon Feb  1 00:56:05 2021
 | INFO: (Computing fibonacci of n...)
Mon Feb  1 00:56:05 2021
 | INFO: (Computed!)
Mon Feb  1 00:56:05 2021
 | INFO: (21 )
Mon Feb  1 00:56:05 2021
 | INFO: (Waiting for user input)
Mon Feb  1 00:56:06 2021
 | INFO: (User entered n = )
Mon Feb  1 00:56:06 2021
 | INFO: (9 )
Mon Feb  1 00:56:06 2021
 | INFO: (Computing fibonacci of n...)
Mon Feb  1 00:56:06 2021
 | INFO: (Computed!)
Mon Feb  1 00:56:06 2021
 | INFO: (34 )
Mon Feb  1 00:56:06 2021
 | INFO: (Waiting for user input)
Mon Feb  1 00:56:07 2021
 | INFO: (User entered n = )
Mon Feb  1 00:56:07 2021
 | INFO: (10 )
Mon Feb  1 00:56:07 2021
 | INFO: (Computing fibonacci of n...)
Mon Feb  1 00:56:07 2021
 | INFO: (Computed!)
Mon Feb  1 00:56:07 2021
 | INFO: (55 )
Mon Feb  1 00:56:07 2021
 | INFO: (Waiting for user input)
Mon Feb  1 00:56:10 2021
 | INFO: (User entered n = )
Mon Feb  1 00:56:10 2021
 | INFO: (20 )
Mon Feb  1 00:56:10 2021
 | INFO: (Computing fibonacci of n...)
Mon Feb  1 00:56:10 2021
 | INFO: (Computed!)
Mon Feb  1 00:56:10 2021
 | INFO: (6765 )
Mon Feb  1 00:56:10 2021
 | INFO: (Waiting for user input)
Mon Feb  1 00:56:12 2021
 | INFO: (User entered n = )
Mon Feb  1 00:56:12 2021
 | INFO: (30 )
Mon Feb  1 00:56:12 2021
 | INFO: (Computing fibonacci of n...)
Mon Feb  1 00:56:12 2021
 | INFO: (Computed!)
Mon Feb  1 00:56:12 2021
 | INFO: (832040 )
Mon Feb  1 00:56:12 2021
 | INFO: (Waiting for user input)
Mon Feb  1 00:56:13 2021
 | INFO: (User entered n = )
Mon Feb  1 00:56:13 2021
 | INFO: (35 )
Mon Feb  1 00:56:13 2021
 | INFO: (Computing fibonacci of n...)
Mon Feb  1 00:56:15 2021
 | INFO: (Computed!)
Mon Feb  1 00:56:15 2021
 | INFO: (9227465 )
Mon Feb  1 00:56:15 2021
 | INFO: (Waiting for user input)
Mon Feb  1 00:56:19 2021
 | INFO: (User entered n = )
Mon Feb  1 00:56:19 2021
 | INFO: (37 )
Mon Feb  1 00:56:19 2021
 | INFO: (Computing fibonacci of n...)
Mon Feb  1 00:56:24 2021
 | INFO: (Computed!)
Mon Feb  1 00:56:24 2021
 | INFO: (24157817 )
Mon Feb  1 00:56:24 2021
 | INFO: (Waiting for user input)
Mon Feb  1 00:56:26 2021
 | INFO: (User entered n = )
Mon Feb  1 00:56:26 2021
 | INFO: (39 )
Mon Feb  1 00:56:26 2021
 | INFO: (Computing fibonacci of n...)
Mon Feb  1 00:56:37 2021
 | INFO: (Computed!)
Mon Feb  1 00:56:37 2021
 | INFO: (63245986 )
Mon Feb  1 00:56:37 2021
 | INFO: (Waiting for user input)
Mon Feb  1 00:56:43 2021
 | INFO: (CLOSING FILE)

*)

let tee f x = f x ; x ;;
type ('a,'err) result = | Ok of 'a | Error of 'err ;;

let result_map f result = match result with | Ok x -> Ok (f x) | Error e -> Error e ;;
let result_bind f result = match result with | Ok x -> f x | Error e -> Error e ;;

type 'a lazy = | Fun of (unit -> 'a) ;;
let lazy_eval (Fun l) = l () ;;
let lazy_map f (Fun l) = Fun (fun () -> l () |> f) ;;
let lazy_bind f (Fun l) = Fun (fun () ->  (l () |> f) |> lazy_eval ) ;;

type 'a stream = | Item of 'a * (unit -> 'a stream) ;;
let rec iota n = Item (n, (fun () -> iota (n+1))) ;;
let rec print_stream n (Item (x,xf)) = if (n=0) then print_str "\n" else print_int x ; print_stream (n-1) (xf()) ;;
let rec map f (Item (x,xf)) = Item (f x,( fun () -> map f (xf()))) ;;
(*
This would play two games
acf_open "log.txt" "w+" |> log (Str "Hello") |> log (Str "This is logging some information") |> tee play_fib |> play_fib;;
*)