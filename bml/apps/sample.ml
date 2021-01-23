let rec print_fibonacci_helper i n f_i f_ip =
    if n<= i then int_println f_i
    else int_print f_i; print_fibonacci_helper (i+1) n f_ip (f_i+f_ip);;

let print_fibonacci n = print_fibonacci_helper 0 n 0 1 ;; (* a cleaner interface *)

let rec countdown n = match n with | 0 -> int_println 0 | _ -> int_println n ; countdown (n-1);;
let rec stack_blowing_countup n = match n with | 0 -> int_println 0 | _ -> stack_blowing_countup (n-1); int_println n ;;

let rec loop i last = (int_println i); (if i=last then () else loop (i+1) last);;
let rec countup n = loop 0 n;;

let rec main () =
    let input = int_scan () in
    if input < 0 then ()
    else countup input; main () ;;

main ();;
