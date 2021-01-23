#include <gtest/gtest.h>
#include <parse/parse.h>
#include <util/message.h>
#include <build/build.h>

std::string load_file(const char *path) {
  std::ifstream f(path);
  return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

enum class direct_test { NONE, COMPILE, LINK, RUN };
void test_build_direct(std::string_view source,
                       std::string_view expected_stdout,
                       int expected_exit_code = 0,
                       std::string_view expected_stderr = "", direct_test mode = direct_test::NONE) {
  if (mode == direct_test::NONE)return;
#define target  "/home/luke/CLionProjects/compilers/bml/output"
  std::ofstream oasm;
  oasm.open(target ".asm");
  ASSERT_NO_THROW(build_direct(source, oasm));
  oasm.close();
  if (mode == direct_test::COMPILE)return;

  ASSERT_EQ(system("yasm  -g dwarf2 -f elf64 " target ".asm -l " target ".lst -o " target ".o"), 0);
  ASSERT_EQ(system("gcc -no-pie " target ".o -o " target), 0);
  if (mode == direct_test::LINK)return;

  int exit_code = WEXITSTATUS(system("timeout 1 " target " 2> " target ".stderr 1> " target ".stdout"));
  EXPECT_EQ(load_file(target ".stdout"), expected_stdout);
  EXPECT_EQ(load_file(target ".stderr"), expected_stderr);
  EXPECT_EQ(exit_code, expected_exit_code);
#undef target
}

void test_build_ir(std::string_view source,
                   std::string_view expected_stdout,
                   int expected_exit_code = 0,
                   std::string_view expected_stderr = ""
) {
#define target  "/home/luke/CLionProjects/compilers/bml/output"
  std::ofstream oasm;
  oasm.open(target ".asm");
  ASSERT_NO_THROW(build_ir(source, oasm));
  oasm.close();
  ASSERT_EQ(system(
      "gcc -c /home/luke/CLionProjects/compilers/bml/lib/rt/rt.c -o /home/luke/CLionProjects/compilers/bml/lib/rt/rt.o -g -O0 "),
            0);
  ASSERT_EQ(system("yasm -g dwarf2 -f elf64 " target ".asm -l " target ".lst -o " target ".o"), 0);
  ASSERT_EQ(system("gcc -no-pie " target ".o /home/luke/CLionProjects/compilers/bml/lib/rt/rt.o -o " target), 0);
  int exit_code = WEXITSTATUS(system("timeout 1 " target " 2> " target ".stderr 1> " target ".stdout"));
  EXPECT_EQ(load_file(target ".stdout"), expected_stdout);
  EXPECT_EQ(load_file(target ".stderr"), expected_stderr);
  EXPECT_EQ(exit_code, expected_exit_code);
#undef target
}

void test_build(std::string_view source,
                std::string_view expected_stdout,
                direct_test mode = direct_test::NONE,
                int expected_exit_code = 0,
                std::string_view expected_stderr = "") {
  test_build_direct(source, expected_stdout, expected_exit_code, expected_stderr, mode);
  test_build_ir(source, expected_stdout, expected_exit_code, expected_stderr);
}

TEST(Build, EmptyProgram) {
  test_build("", "", direct_test::RUN);
}

TEST(Build, SomeAllocations) {
  test_build("type int_option = | None | Some of int ;;\n"
             "let tuple = (1,2,3)\n"
             "and some_int = Some 3\n"
             "and none = None\n"
             "and three = 3\n"
             ";;", "", direct_test::RUN);
}

TEST(Build, Expression0) {
  constexpr std::string_view source = "let answer = 42;;"
                                      "int_print 42;;\n"
                                      "int_print answer;;\n";
  test_build(source, "42 42 ", direct_test::RUN);

}

TEST(Build, Expression0_1) {
  constexpr std::string_view source = "int_print (10+20);;\n";
  test_build(source, "30 ", direct_test::RUN);
}

TEST(Build, Expression0_2) {
  constexpr std::string_view source = "let cond = false;;\n"
                                      "int_print (if cond then 42 else 1729);;\n";
  test_build(source, "1729 ", direct_test::RUN);
}

TEST(Build, Expression0_3) {
  constexpr std::string_view source = "let a_pair = (92, 54) ;;\n"
                                      "let (x,y) = a_pair;;\n"
                                      "int_print x; int_print y;;\n";
  test_build(source, "92 54 ", direct_test::RUN);
}

TEST(Build, Expression0_4) {
  constexpr std::string_view source = "int_print (if 107 = 106 then 10 else 12);;\n";
  test_build(source, "12 ", direct_test::RUN);
}

TEST(Build, PartialApplication) {
  constexpr std::string_view source = "let s100 = (+) 100;;\n"
                                      "int_print (s100 54);;\n";
  test_build(source, "154 ", direct_test::RUN);
}

TEST(Build, Expression0_5) {
  constexpr std::string_view source = "let a_pair = (92, 54) ;;\n"
                                      "let (x,y) = a_pair;;\n"
                                      "let sum_100 = (+) 100 ;;\n"
                                      "int_print (sum_100 54);;";
  test_build(source, "154 ", direct_test::RUN);
}

TEST(Build, Expression1) {
  constexpr std::string_view source = "let answer = 42 ;;\n"
                                      "let another_answer = answer;;\n"
                                      "let answer = 56\n"
                                      "and yet_another_answer = answer;;\n"
                                      "let a_pair = (92, 54) ;;\n"
                                      "let (x,y) = a_pair;;\n"
                                      "let sum_100 = (+) 100 ;;\n"
                                      "int_print (sum_100 (if 107=106 then x else y)) ;;\n";
  test_build(source, "154 ", direct_test::RUN);

}

TEST(Build, Expression2) {
  constexpr std::string_view source = "let g f x = f x 1 ;;\n"
                                      "let y = g (+) 10;;\n"
                                      "int_print y;;";
  test_build(source, "11 ", direct_test::RUN);
}

TEST(Build, Expression3) {
  constexpr std::string_view source = "let twice x = x + x ;;\n"
                                      "let ans = 45 + 25 - 93 + (twice (97 + 21));;\n"
                                      "int_print ans;;";
  test_build(source, "213 ", direct_test::RUN);
}

TEST(Build, Expression4) {
  test_build("let f x () = int_print x ;;\n"
             "let g = f 42;;\n"
             "g ();;", "42 ", direct_test::RUN);
}

TEST(Build, BoollLiterals) {
  test_build(R"(
    int_println (if true then 42 else 55);;
    int_println (if false then 42 else 55);;
  )", "42\n55\n", direct_test::RUN);
}

TEST(Build, Constructor) {
  test_build("type int_option = | None | Some of int ;;\n"
             "let some13 = Some 13 and none = None;;\n"
             "let Some 13 = some13;;\n"
             "let Some x = some13;;\n"
             "int_print x;;\n", "13 ", direct_test::RUN);
}

TEST(Build, SomeAddition) {
  test_build("type int_option = | Some of int ;;\n"
             "let some_add (Some x) (Some y) (Some z) = Some (x+y+z);;\n"
             "let Some ans = some_add (Some 10) (Some 100) (Some 1);;\n"
             "int_print ans;;\n", "111 ", direct_test::RUN);
}

TEST(Build, OptionMapSome) {
  test_build("type int_option = | None | Some of int ;;\n"
             "let option_map f xo = match xo with | None -> None | Some x -> Some (f x);;\n"
             "let _ = option_map int_print (Some 42);;\n", "42 ", direct_test::RUN);

}

TEST(Build, OptionMapNone) {
  test_build("type int_option = | None | Some of int ;;\n"
             "let option_map f xo = match xo with | None -> None | Some x -> Some (f x);;\n"
             "let _ = option_map int_print None;;\n", "", direct_test::RUN);
}

TEST(Build, OptionMapError) {
  test_build("type int_option = | None | Some of int | Another ;;\n"
             "let option_map f xo = match xo with | None -> None | Some x -> Some (f x);;\n"
             "let _ = option_map int_print Another;;\n", "", direct_test::RUN, 1, "match failed\n");
}

TEST(Build, ApplyTwice) {
  test_build("let apply_twice f x = f (f x);;\n"
             "let plus_two = apply_twice (fun x -> x + 1);;\n"
             "int_print (plus_two 40);;\n", "42 ", direct_test::RUN);
}

TEST(Build, ApplyTwiceOnSteroids) {
  test_build("let apply_twice f x = f (f x);;\n"
             "int_print (apply_twice apply_twice (fun x -> x + 1) 0);;\n", "4 ", direct_test::RUN);
}

TEST(Build, FnCompose) {
  test_build("let fn_compose g f x = g (f x);;\n"
             "int_print (fn_compose (fun x -> x+x) (fun x -> x + 1) 12);;\n", "26 ", direct_test::RUN);
}

TEST(Build, OptionMap) {
  test_build("type int_option = | None | Some of int ;;\n"
             "let option_map f x = match x with | None -> None | Some x -> Some (f x);;\n"
             "let maybe_add y x = match y with | None -> None | Some y -> Some (x+y);;"
             "let Some ans = maybe_add (Some 10) 1;;\n"
             "int_print ans;;\n", "11 ", direct_test::RUN);
}

TEST(Build, MaybeAdditionNested) {
  test_build("type int_option = | None | Some of int ;;\n"
             "let option_map f x = match x with | None -> None | Some x -> Some (f x);;\n"
             "let maybe_add y x = match y with | None -> None | Some y -> Some (x+y);;\n"
             "let maybe_sum x y = option_map (maybe_add y) x;;\n"
             "let Some (Some ans) = maybe_sum (Some 10) (Some 100);;\n"
             "int_print ans;;\n", "110 ", direct_test::RUN);
}

TEST(Build, MaybeAdditionCorrect) {
  test_build("type int_option = | None | Some of int ;;\n"
             "let option_map f x = match x with | None -> None | Some x -> Some (f x);;\n"
             "let option_bind f x = match x with | None -> None | Some x -> f x;;\n"
             "let maybe_add y x = match y with | None -> None | Some y -> Some (x+y);;\n"
             "let maybe_sum x y = option_bind (maybe_add y) x;;\n"
             "let maybe_print x = match x with | None -> int_print (-1) | Some x -> int_print x;;\n"
             "let _ = maybe_print (maybe_sum (Some 10) (Some 100));;\n"
             "let _ = maybe_print (maybe_sum (None) (Some 100));;\n"
             "let _ = maybe_print (maybe_sum (Some 10) (None));;\n"
             "let _ = maybe_print (maybe_sum (None) (None));;\n", "110 -1 -1 -1 ", direct_test::RUN);
}

TEST(Build, MaybeAdditionWithCapture) {
  test_build("type int_option = | None | Some of int ;;\n"
             "let option_map f x = match x with | None -> None | Some x -> Some (f x);;\n"
             "let option_bind f x = match x with | None -> None | Some x -> f x;;\n"
             "let maybe_print x = match x with | None -> int_print (-1) | Some x -> int_print x;;\n"
             "let maybe_sum maybe_a maybe_b = option_bind (fun a -> option_map (fun b -> (a+b) ) maybe_b) maybe_a;;\n"
             "let test_sum a b = maybe_print (maybe_sum a b);;\n"
             "let _ = test_sum (Some 10) (Some 100);;\n"
             "let _ = test_sum (None) (Some 100);;\n"
             "let _ = test_sum (Some 10) (None);;\n"
             "let _ = test_sum (None) (None);;\n", "110 -1 -1 -1 ", direct_test::RUN);
}

TEST(Build, LetExpression) {
  test_build("let play_with x = int_print x; (let y = x + x in y);;\n"
             "let ans = play_with 10;;"
             "int_print ans;;", "10 20 ", direct_test::RUN);
}

TEST(Build, Malloc) {
  test_build("type option = | Some of 'a;;\n"
             "let heap_big_tuple = Some (1,2,3,4,5,6,7);;\n"
             "let stck_big_tuple = 1,2,3,4,5,6,7;;", "", direct_test::RUN);
}

TEST(Build, CaptureX) {
  test_build("let plus_x x = (fun y -> x + y);;\n"
             "let plus_3 = plus_x 3;;\n"
             "int_print (plus_3 4);;", "7 ", direct_test::RUN);
}

TEST(Build, CaptureMaybeSum) {
  test_build("type int_option = | None | Some of int ;;\n"
             "let option_map f x = match x with | None -> None | Some x -> Some (f x);;\n"
             "let option_bind f x = match x with | None -> None | Some x -> f x;;\n"
             "let maybe_sum y x = option_bind (fun x -> option_map (fun y -> x + y) y) x;;\n"
             "let maybe_print x = match x with | None -> int_print (-1) | Some x -> int_print x;;\n"
             "let _ = maybe_print (maybe_sum (Some 10) (Some 100));;\n"
             "let _ = maybe_print (maybe_sum (None) (Some 100));;\n"
             "let _ = maybe_print (maybe_sum (Some 10) (None));;\n"
             "let _ = maybe_print (maybe_sum (None) (None));;\n", "110 -1 -1 -1 ", direct_test::RUN);
}

TEST(Build, LongSum) {
  test_build("let long_sum a b c d e f g h i j k = a+b+c+d+e+f+g+h+i+j+k;;\n"
             "let ans = long_sum 1 2 3 4 5 6 7 8 9 10 11;;\n"
             "int_print ans ;;", "66 ", direct_test::RUN);
}

TEST(Build, DeepCapture) {
  test_build("let deep_capture x = fun () -> fun () -> fun () -> fun () -> fun () -> fun () -> fun () -> x ;;"
             "      int_print (deep_capture 1729 () () () () () () ());;", "1729 ", direct_test::RUN);
}

TEST(Build, ManyMatchers) {
  test_build("type t = | Void | Int of int | Tuple of int * t ;;\n"
             "let f x = match x with\n"
             "| Void -> 0\n"
             "| Int 0 -> 1\n"
             "| Int 1 -> 2\n"
             "| Int x -> 3\n"
             "| Tuple (0,_) -> 4\n"
             "| Tuple (x,Void) -> 5 + x\n"
             "| Tuple (x,Int y) -> 6 + x + y\n"
             "| Tuple (x,Tuple (y, Tuple (z,_))) -> 7 + x + y +z\n"
             "| Tuple (x, t) -> 8 + x;;\n"
             "let g x = int_print (f x);;\n"
             "g Void;;\n"
             "g (Int 0);;\n"
             "g (Int 1);;\n"
             "g (Int 64);;\n"
             "g (Tuple (0,Int 3));;\n"
             "g (Tuple (1000,Void));;\n"
             "g (Tuple (1000,Int 100));;\n"
             "g (Tuple (1000,Tuple(10000, Tuple(100,Void))));;\n"
             "g (Tuple (1000,Void));;\n", "0 1 2 3 4 1005 1106 11107 1005 ", direct_test::RUN);
}

TEST(Build, DeepMatchers) {
  test_build("let f ((x,y),z) = x + y + z;;\n"
             "int_print (f ((1,10),100));;\n", "111 ");
  test_build("let f (((((a,b),c),d),e),f) = a+b+c+d+e+f ;;\n"
             "int_print (f  (((((1,2),4),8),16),32) );;\n", "63 ", direct_test::RUN);
}

TEST(Build, MatchersTheRevenge) {
  test_build("type t = | Null | Triple of (int * int) * int;;\n"
             "let f x = match x with\n"
             "| Null -> 0\n"
             "| Triple ((x,y),z) -> x + y + z;;\n"
             "let g x = int_print (f x);;\n"
             "g Null;;\n"
             "g (Triple ((1,10),100));;\n", "0 111 ", direct_test::RUN);
}

TEST(Build, ListUtils1) {
  test_build("type 'a list = | Null | Cons of 'a * 'a list;;\n"
             "let rec length l = match l with\n"
             "| Null -> 0\n"
             "| Cons (_,xs) -> (length xs) + 1;;\n"
             "int_print (length  (  Cons(1,Cons(2,Cons(3,Null))) ) );;", "3 ", direct_test::RUN);
}

TEST(Build, ListUtils2) {
  test_build("type 'a list = | Null | Cons of 'a * 'a list;;\n"
             "let rec print_list l = match l with\n"
             "| Null -> ()\n"
             "| Cons (x,xs) -> int_print x; print_list xs;;\n"
             "print_list  (Cons(1,Cons(2,Cons(3,Null)))) ;;", "1 2 3 ", direct_test::RUN);
}

TEST(Build, InfiniteList) {
  test_build("type 'a list = | Null | Cons of 'a * 'a list;;\n"
             "let rec length l = match l with\n"
             "| Null -> 0\n"
             "| Cons (_,xs) -> (length xs) + 1;;\n"
             "let rec a = Cons(1,a);;\n"
             "int_print (length a) ;;",
             "",
             direct_test::RUN,
             139,
             "timeout: the monitored command dumped core\nSegmentation fault\n");
}

TEST(Build, TakeFromInfiniteList) {
  test_build("type 'a list = | Null | Cons of 'a * 'a list;;\n"
             "let rec print_list l = match l with\n"
             "| Null -> ()\n"
             "| Cons (x,xs) -> int_print x; print_list xs;;\n"
             "let rec take l n = if n=0 then Null else match l with\n"
             "| Null -> Null\n"
             "| Cons (x,xs) -> Cons (x,take xs (n-1))\n;;"
             "let rec a = Cons(1,a);;\n"
             " print_list (take a 4) ;;\n"
             "let rec b = Cons(2,c) and c = Cons(3,b);;\n"
             " print_list (take b 8) ;;", "1 1 1 1 2 3 2 3 2 3 2 3 ", direct_test::RUN);

}

#define floyd_algo "    let rec run_until_equal f tortoise hare =\n"\
"    if tortoise=hare then (tortoise,hare)\n"\
"    else run_until_equal f (f tortoise) (f (f hare)) ;;\n"\
\
"    let rec find_mu f tortoise hare mu =\n"\
"    if tortoise=hare then (tortoise,hare,mu)\n"\
"    else find_mu f (f tortoise) (f hare) (mu+1) ;;\n"\
\
"    let rec find_lam f tortoise hare lam = \n"\
"    if tortoise=hare then lam else\n"\
"    find_lam f tortoise (f hare) (lam +1) ;;\n"\
\
"let floyd f x0 = \n"\
"    let tortoise = f x0 in\n"\
"    let hare = f tortoise in\n"\
"    let tortoise,hare = run_until_equal f tortoise hare in\n"\
"    let tortoise,hare,mu = find_mu f x0 hare 0 in\n"\
"    let lam = find_lam f tortoise (f tortoise) 1 in\n"\
"    (lam,mu)        ;;\n"

TEST(Build, TortoiseAndHare_Numbers) {

  test_build(
      floyd_algo

      "let (lam,mu) = floyd (fun x -> match x with | 10 -> 5 | x -> x+1) 0;;\n"
      "int_print lam; int_print mu;;\n", "6 5 ", direct_test::RUN);
}

TEST(Build, TortoiseAndHare_Simple) {
  test_build(
      floyd_algo
      "type 'a list = | Null | Cons of 'a * 'a list;;\n"

      "let list_examine_cycle l = \n"
      " let (lam,mu) = floyd (fun Cons(_,xs) -> xs) l in\n"
      "int_print lam; int_print mu;;\n"

      "let rec a = Cons(10,b) and b = Cons(20,a) and c = Cons (1 , Cons (2, Cons (3, Cons(4,a)) ) );;\n"

      "list_examine_cycle c;;", "2 4 ", direct_test::RUN);
}

TEST(Build, SmallComparison) {
  test_build(R"(
 int_print (3 < 4);;
    int_print (3 < 3);;
    int_println (3 < 2);;
)", "1 0 0\n", direct_test::RUN);
}

TEST(Build, IntComparison) {
  test_build(R"(
    int_print (3 < 4);;
    int_print (3 < 3);;
    int_println (3 < 2);;

    int_print (0 < (-1));;
    int_print (0 < 0);;
    int_println (0 < 1);;

    int_print ((-1) < (-2));;
    int_print ((-1) < (-1));;
    int_println ((-1) <  0);;

    int_println ((-54) < (-53));;

)", "1 0 0\n0 0 1\n0 0 1\n1\n", direct_test::RUN);
}

TEST(Build, Stream) {
  test_build(R"(
  type 'a seq = | Item of 'a * (unit -> 'a seq);;
  let rec print_int_stream n (Item (x,xf)) =
    if n=0
    then (
      int_println x
    ) else (
      int_print x; print_int_stream (n-1) (xf())
    )
  ;;

  let rec iota n = Item (n, (fun () -> iota (n+1)) );;
  print_int_stream 10 (iota 42);;

  let rec map f (Item (x,xf)) = Item (f x, (fun () -> map f (xf())) );;
  print_int_stream 10 (map (fun x -> x+x+1) (iota 42));;

  let rec filter p (Item (x,xf)) =
    if (p x) then (
      Item (x, (fun () -> filter p (xf())) )
    ) else (
      filter p (xf())
    );;

  print_int_stream 10 (filter (fun x -> 50 < x) (iota 42));;

  let rec partial_sum f init
 (Item(x,xf)) =
    let
      init = f init x
    in
    Item(init, (fun () -> partial_sum f init (xf())));;

  print_int_stream 10 (partial_sum (+) 0 (iota 1)) ;;

  (* let not_divisibly_by d n = not ((n mod d) = 0) ;; *)

                      )",
             "42 43 44 45 46 47 48 49 50 51 52\n"
             "85 87 89 91 93 95 97 99 101 103 105\n"
             "51 52 53 54 55 56 57 58 59 60 61\n"
             "1 3 6 10 15 21 28 36 45 55 66\n", direct_test::RUN);
}

TEST(Build, ArithmeticExpression) {
  test_build("int_println (4*5+27/34*32-27*34/32+5*(6+23)/45);;", "-5\n");
}
/*

*/
//Test IDEA: let rec x = Some y and y = f ();;
