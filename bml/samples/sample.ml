(* First, define the library for the autoclosefile.
    We don't have modules yet, so we'll just prepend
  acf_ to the functions of this "module" for clarity.*)

type file = int;;
type time = int;;

(*Module Auto_close_file *)

    (* These two should be not exposed *)
    type acf_t = | Fd of file;;
    let acf_fd (Fd fd) = fd;;

    let acf_open path mode = (Fd (fopen path mode)) ~> (fun (Fd fd) -> fclose fd);;
    let acf_open_msg path mode msg = (Fd (fopen path mode)) ~> (fun (Fd fd) -> fprint_str fd msg; fclose fd);;
    let acf_print_str acf str = fprint_str (acf_fd acf) str; acf;;
    let acf_print_int acf n = fprintln_int (acf_fd acf) n; acf;;
    type acf_poly_t = | Int of int | Str of string | Time of time ;;
    let rec acf_print_poly acf x =
      (match x with
        | (Int n) -> fprintln_int (acf_fd acf) n
        | (Str s) -> fprint_str (acf_fd acf) s
        | (Time t) -> fprint_time (acf_fd acf) t);
      acf_print_poly acf ;; (* This function doesn't actually typechek in HM, but it had a cool signature for this example *)
(* end module *)

let make_logger () =
  let f = acf_open_msg "/tmp/log.txt" "w+" "##### Closing log file #####\n" in
  acf_print_str f "##### Opening log file #####\n"
  ; acf_print_poly f (Str "Time is: ") (Time (now())) (Int 34)  (Str "Hello\n") (Int 62875)
;;

make_logger ();; (* The Acf gets destroyed, and the file get closed *)
make_logger () (Int 1729) (Str "As long as we use it, the file is still alive!\n");; (* This time it survives a bit more *)
