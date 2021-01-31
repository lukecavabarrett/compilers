#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <parse/parse.h>
#include <util/message.h>
#include <build/build.h>

std::string load_file(const char *path) {
  std::ifstream f(path);
  return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

typedef std::variant<std::string_view, ::testing::PolymorphicMatcher<testing::internal::MatchesRegexMatcher> >
    expect_str_t;

struct test_params {
  bool use_valgrind = false; // takes longer time
  bool sandbox_timeout = false;
  bool use_release_lib = true; // true for testing efficiency; false for debugging
  expect_str_t expected_stdout = "";
  expect_str_t expected_stderr = "";
  int expected_exit_code = 0;
};

void compile_lib_debug() {
  static bool compiled = false;
  if (compiled)return;
  compiled = true;
  ASSERT_EQ(system(
      "gcc -c /home/luke/CLionProjects/compilers/bml/lib/rt/rt.c -o /home/luke/CLionProjects/compilers/bml/lib/rt/rt.o -g -O0 -Wall"),
            0);
}

void compile_lib_release() {
  static bool compiled = false;
  if (compiled)return;
  compiled = true;
  ASSERT_EQ(system(
      "gcc -c /home/luke/CLionProjects/compilers/bml/lib/rt/rt.c -o /home/luke/CLionProjects/compilers/bml/lib/rt/rt_fast.o -O3 -Wall"),
            0);
}

void test_build(std::string_view source, test_params tp) {

  if (tp.sandbox_timeout && tp.use_valgrind) {
    std::cerr << "Valgrind and timeout are not available at the same time - deactivating valgrind for this test."
              << std::endl;
    tp.use_valgrind = false;
  }
  if (tp.use_release_lib)compile_lib_release();
  else compile_lib_debug();
#define target  "/home/luke/CLionProjects/compilers/bml/output"
  std::ofstream oasm;
  oasm.open(target ".asm");
  ASSERT_NO_THROW(build_ir(source, oasm));
  oasm.close();

  ASSERT_EQ(system("yasm -g dwarf2 -f elf64 " target ".asm -l " target ".lst -o " target ".o"), 0);
  if (tp.use_release_lib)
    ASSERT_EQ(system("gcc -no-pie " target ".o /home/luke/CLionProjects/compilers/bml/lib/rt/rt_fast.o -o " target), 0);
  else
    ASSERT_EQ(system("gcc -no-pie " target ".o /home/luke/CLionProjects/compilers/bml/lib/rt/rt.o -o " target), 0);

  int exit_code;
  if (tp.sandbox_timeout)
    exit_code = WEXITSTATUS(system("timeout 1 " target " 2> " target ".stderr 1> " target ".stdout"));
  else if (tp.use_valgrind)
    exit_code =
        WEXITSTATUS(system("/usr/bin/valgrind  --tool=memcheck --quiet " " --log-file=" target ".valgrind.log " " --gen-suppressions=all --leak-check=full --leak-resolution=med --track-origins=yes --vgdb=no " target " 2> " target ".stderr " " 1> " target ".stdout"));
  else exit_code = WEXITSTATUS(system(target " 2> " target ".stderr 1> " target ".stdout"));
  std::visit(util::overloaded{
      [](std::string_view expected_stdout) { EXPECT_EQ(load_file(target ".stdout"), expected_stdout); },
      [](const auto &expected_stdout) { EXPECT_THAT(load_file(target ".stdout"), expected_stdout); }
  }, tp.expected_stdout);
  std::visit(util::overloaded{
      [](std::string_view expected_stderr) { EXPECT_EQ(load_file(target ".stderr"), expected_stderr); },
      [](const auto &expected_stderr) { EXPECT_THAT(load_file(target ".stderr"), expected_stderr); }
  }, tp.expected_stderr);
  if (tp.use_valgrind)EXPECT_EQ(load_file(target ".valgrind.log"), "");
  EXPECT_EQ(exit_code, tp.expected_exit_code);
#undef target
}

TEST(Build, EmptyProgram) {
  test_build("", {});
}

TEST(Build, SomeAllocations) {
  test_build("type int_option = | None | Some of int ;;\n"
             "let tuple = (1,2,3)\n"
             "and some_int = Some 3\n"
             "and none = None\n"
             "and three = 3\n"
             ";;", {});
}

TEST(Build, Expression0) {
  constexpr std::string_view source = "let answer = 42;;"
                                      "print_int 42;;\n"
                                      "print_int answer;;\n";
  test_build(source, {.expected_stdout = "42 42 "});

}

TEST(Build, Expression0_1) {
  constexpr std::string_view source = "println_int (10+20);;\n";
  test_build(source, {.expected_stdout = "30\n"});
}

TEST(Build, Expression0_2) {
  constexpr std::string_view source = "let cond = false;;\n"
                                      "println_int (if cond then 42 else 1729);;\n";
  test_build(source, {.expected_stdout = "1729\n"});
}

TEST(Build, Expression0_3) {
  constexpr std::string_view source = "let a_pair = (92, 54) ;;\n"
                                      "let (x,y) = a_pair;;\n"
                                      "print_int x; print_int y;;\n";
  test_build(source, {.expected_stdout = "92 54 "});
}

TEST(Build, Expression0_4) {
  constexpr std::string_view source = "print_int (if 107 = 106 then 10 else 12);;\n";
  test_build(source, {.expected_stdout = "12 "});
}

TEST(Build, PartialApplication) {
  constexpr std::string_view source = "let s100 = (+) 100;;\n"
                                      "print_int (s100 54);;\n";
  test_build(source, {.expected_stdout = "154 "});
}

TEST(Build, Expression0_5) {
  constexpr std::string_view source = "let a_pair = (92, 54) ;;\n"
                                      "let (x,y) = a_pair;;\n"
                                      "let sum_100 = (+) 100 ;;\n"
                                      "print_int (sum_100 54);;";
  test_build(source, {.expected_stdout = "154 "});
}

TEST(Build, Expression1) {
  constexpr std::string_view source = "let answer = 42 ;;\n"
                                      "let another_answer = answer;;\n"
                                      "let answer = 56\n"
                                      "and yet_another_answer = answer;;\n"
                                      "let a_pair = (92, 54) ;;\n"
                                      "let (x,y) = a_pair;;\n"
                                      "let sum_100 = (+) 100 ;;\n"
                                      "print_int (sum_100 (if 107=106 then x else y)) ;;\n";
  test_build(source, {.expected_stdout = "154 "});

}

TEST(Build, Expression2) {
  constexpr std::string_view source = "let g f x = f x 1 ;;\n"
                                      "let y = g (+) 10;;\n"
                                      "print_int y;;";
  test_build(source, {.expected_stdout = "11 "});
}

TEST(Build, Expression3) {
  constexpr std::string_view source = "let twice x = x + x ;;\n"
                                      "let ans = 45 + 25 - 93 + (twice (97 + 21));;\n"
                                      "print_int ans;;";
  test_build(source, {.expected_stdout = "213 "});
}

TEST(Build, Expression4) {
  test_build("let f x () = print_int x ;;\n"
             "let g = f 42;;\n"
             "g ();;", {.expected_stdout = "42 "});
}

TEST(Build, BoollLiterals) {
  test_build(R"(
    println_int (if true then 42 else 55);;
    println_int (if false then 42 else 55);;
  )", {.expected_stdout = "42\n55\n"});
}

TEST(Build, Constructor) {
  test_build("type int_option = | None | Some of int | More of int * int  ;;\n"
             "let some13 = Some 13 and none = None and more42_56 = More (42,56);;\n"
             "let Some 13 = some13;;\n"
             "let Some x = some13;;\n"
             "let More (y,z) = more42_56;;\n"
             "print_int x; print_int y; println_int z;;\n", {.expected_stdout = "13 42 56\n"});
}

TEST(Build, SomeAddition) {
  test_build("type int_option = | Some of int ;;\n"
             "let some_add (Some x) (Some y) (Some z) = Some (x+y+z);;\n"
             "let Some ans = some_add (Some 10) (Some 100) (Some 1);;\n"
             "print_int ans;;\n", {.expected_stdout = "111 "});
}

TEST(Build, OptionMapSome) {
  test_build("type int_option = | None | Some of int ;;\n"
             "let option_map f xo = match xo with | None -> None | Some x -> Some (f x);;\n"
             "let _ = option_map print_int (Some 42);;\n", {.expected_stdout = "42 "});

}

TEST(Build, OptionMapNone) {
  test_build("type int_option = | None | Some of int ;;\n"
             "let option_map f xo = match xo with | None -> None | Some x -> Some (f x);;\n"
             "let _ = option_map print_int None;;\n", {});
}

TEST(Build, OptionMapError) {
  test_build("type int_option = | None | Some of int | Another ;;\n"
             "let option_map f xo = match xo with | None -> None | Some x -> Some (f x);;\n"
             "let _ = option_map print_int Another;;\n",
             {.expected_stderr = "match failed\n", .expected_exit_code=1});
}

TEST(Build, BigConstrPrinting) {
  test_build(R"(
type t = | A of int * unit * bool ;;
let make () = (A (3,(),true)) ~> (fun _ -> print_str "Destroyed!\n" );;
let fst (A(x,_,_)) = x;;
make () |> fst |> println_int ;;
)", {.expected_stdout = "Destroyed!\n3\n", .expected_exit_code=0});
  test_build(R"(
type t = | A of int * unit * bool ;;
let make () = (A (3,(),true)) ~> (fun _ -> print_str "Destroyed!\n" );;
let fst (A(x,_,_)) = x;;
let abc = make ();;
println_int (fst abc) ;;
(* globals don't get destroyed - for now *)
)", {.expected_stdout = "3\n", .expected_exit_code=0});
}

TEST(Build, ApplyTwice) {
  test_build("let apply_twice f x = f (f x);;\n"
             "let plus_two = apply_twice (fun x -> x + 1);;\n"
             "print_int (plus_two 40);;\n", {.expected_stdout = "42 "});
}

TEST(Build, ApplyTwiceOnSteroids) {
  test_build("let apply_twice f x = f (f x);;\n"
             "print_int (apply_twice apply_twice (fun x -> x + 1) 0);;\n", {.expected_stdout = "4 "});
}

TEST(Build, FnCompose) {
  test_build("let fn_compose g f x = g (f x);;\n"
             "print_int (fn_compose (fun x -> x+x) (fun x -> x + 1) 12);;\n",
             {.expected_stdout = "26 "});
}

TEST(Build, OptionMap) {
  test_build("type int_option = | None | Some of int ;;\n"
             "let option_map f x = match x with | None -> None | Some x -> Some (f x);;\n"
             "let maybe_add y x = match y with | None -> None | Some y -> Some (x+y);;"
             "let Some ans = maybe_add (Some 10) 1;;\n"
             "print_int ans;;\n", {.expected_stdout = "11 "});
}

TEST(Build, MaybeAdditionNested) {
  test_build("type int_option = | None | Some of int ;;\n"
             "let option_map f x = match x with | None -> None | Some x -> Some (f x);;\n"
             "let maybe_add y x = match y with | None -> None | Some y -> Some (x+y);;\n"
             "let maybe_sum x y = option_map (maybe_add y) x;;\n"
             "let Some (Some ans) = maybe_sum (Some 10) (Some 100);;\n"
             "print_int ans;;\n", {.expected_stdout = "110 "});
}

TEST(Build, MaybeAdditionCorrect) {
  test_build("type int_option = | None | Some of int ;;\n"
             "let option_map f x = match x with | None -> None | Some x -> Some (f x);;\n"
             "let option_bind f x = match x with | None -> None | Some x -> f x;;\n"
             "let maybe_add y x = match y with | None -> None | Some y -> Some (x+y);;\n"
             "let maybe_sum x y = option_bind (maybe_add y) x;;\n"
             "let maybe_print x = match x with | None -> print_int (-1) | Some x -> print_int x;;\n"
             "let _ = maybe_print (maybe_sum (Some 10) (Some 100));;\n"
             "let _ = maybe_print (maybe_sum (None) (Some 100));;\n"
             "let _ = maybe_print (maybe_sum (Some 10) (None));;\n"
             "let _ = maybe_print (maybe_sum (None) (None));;\n",
             {.expected_stdout = "110 -1 -1 -1 "});
}

TEST(Build, MaybeAdditionWithCapture) {
  test_build("type int_option = | None | Some of int ;;\n"
             "let option_map f x = match x with | None -> None | Some x -> Some (f x);;\n"
             "let option_bind f x = match x with | None -> None | Some x -> f x;;\n"
             "let maybe_print x = match x with | None -> print_int (-1) | Some x -> print_int x;;\n"
             "let maybe_sum maybe_a maybe_b = option_bind (fun a -> option_map (fun b -> (a+b) ) maybe_b) maybe_a;;\n"
             "let test_sum a b = maybe_print (maybe_sum a b);;\n"
             "let _ = test_sum (Some 10) (Some 100);;\n"
             "let _ = test_sum (None) (Some 100);;\n"
             "let _ = test_sum (Some 10) (None);;\n"
             "let _ = test_sum (None) (None);;\n", {.expected_stdout = "110 -1 -1 -1 "});
}

TEST(Build, LetExpression) {
  test_build("let play_with x = print_int x; (let y = x + x in y);;\n"
             "let ans = play_with 10;;"
             "print_int ans;;", {.expected_stdout = "10 20 "});
}

TEST(Build, Malloc) {
  test_build("type 'a option = | Some of 'a;;\n"
             "let heap_big_tuple = Some (1,2,3,4,5,6,7);;\n"
             "let stck_big_tuple = 1,2,3,4,5,6,7;;", {});
}

TEST(Build, CaptureX) {
  test_build("let plus_x x = (fun y -> x + y);;\n"
             "let plus_3 = plus_x 3;;\n"
             "print_int (plus_3 4);;", {.expected_stdout = "7 "});
}

TEST(Build, CaptureMaybeSum) {
  test_build("type int_option = | None | Some of int ;;\n"
             "let option_map f x = match x with | None -> None | Some x -> Some (f x);;\n"
             "let option_bind f x = match x with | None -> None | Some x -> f x;;\n"
             "let maybe_sum y x = option_bind (fun x -> option_map (fun y -> x + y) y) x;;\n"
             "let maybe_print x = match x with | None -> print_int (-1) | Some x -> print_int x;;\n"
             "let _ = maybe_print (maybe_sum (Some 10) (Some 100));;\n"
             "let _ = maybe_print (maybe_sum (None) (Some 100));;\n"
             "let _ = maybe_print (maybe_sum (Some 10) (None));;\n"
             "let _ = maybe_print (maybe_sum (None) (None));;\n",
             {.expected_stdout = "110 -1 -1 -1 "});
}

TEST(Build, LongSum) {
  test_build("let long_sum a b c d e f g h i j k = a+b+c+d+e+f+g+h+i+j+k;;\n"
             "let ans = long_sum 1 2 3 4 5 6 7 8 9 10 11;;\n"
             "print_int ans ;;", {.expected_stdout = "66 "});
}

TEST(Build, DeepCapture) {
  test_build("let deep_capture x = fun () -> fun () -> fun () -> fun () -> fun () -> fun () -> fun () -> x ;;"
             "      print_int (deep_capture 1729 () () () () () () ());;",
             {.expected_stdout = "1729 "});
}

TEST(Build, ConstructorMatchers) {
  test_build(R"(
type t = | A of int | B of (int * int) | C of int * int ;;
let f x = match x with
| C (x,y) -> 100 + x + y;;
println_int (f (C (1,2)));;
)", {.expected_stdout="103\n"});
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
             "let g x = print_int (f x);;\n"
             "g Void;;\n"
             "g (Int 0);;\n"
             "g (Int 1);;\n"
             "g (Int 64);;\n"
             "g (Tuple (0,Int 3));;\n"
             "g (Tuple (1000,Void));;\n"
             "g (Tuple (1000,Int 100));;\n"
             "g (Tuple (1000,Tuple(10000, Tuple(100,Void))));;\n"
             "g (Tuple (1000,Void));;\n", {.expected_stdout = "0 1 2 3 4 1005 1106 11107 1005 "});
}

TEST(Build, DeepMatchers) {
  test_build("let f ((x,y),z) = x + y + z;;\n"
             "print_int (f ((1,10),100));;\n", {.expected_stdout = "111 "});
  test_build("let f (((((a,b),c),d),e),f) = a+b+c+d+e+f ;;\n"
             "print_int (f  (((((1,2),4),8),16),32) );;\n", {.expected_stdout = "63 "});
}

TEST(Build, MatchersTheRevenge) {
  test_build("type t = | Null | Triple of (int * int) * int;;\n"
             "let f x = match x with\n"
             "| Null -> 0\n"
             "| Triple ((x,y),z) -> x + y + z;;\n"
             "let g x = print_int (f x);;\n"
             "g Null;;\n"
             "g (Triple ((1,10),100));;\n", {.expected_stdout = "0 111 "});
}

TEST(Build, ListUtils1) {
  test_build("type 'a list = | Null | Cons of 'a * 'a list;;\n"
             "let rec length l = match l with\n"
             "| Null -> 0\n"
             "| Cons (_,xs) -> (length xs) + 1;;\n"
             "print_int (length  (  Cons(1,Cons(2,Cons(3,Null))) ) );;", {.expected_stdout = "3 "});
}

TEST(Build, ListUtils2) {
  test_build("type 'a list = | Null | Cons of 'a * 'a list;;\n"
             "let rec print_list l = match l with\n"
             "| Null -> ()\n"
             "| Cons (x,xs) -> print_int x; print_list xs;;\n"
             "print_list  (Cons(1,Cons(2,Cons(3,Null)))) ;;", {.expected_stdout = "1 2 3 "});
}

TEST(Build, InfiniteList) {
  test_build("type 'a list = | Null | Cons of 'a * 'a list;;\n"
             "let rec length l = match l with\n"
             "| Null -> 0\n"
             "| Cons (_,xs) -> (length xs) + 1;;\n"
             "let rec a = Cons(1,a);;\n"
             "print_int (length a) ;;",
             {.sandbox_timeout=true, .expected_stdout = "", .expected_stderr = "timeout: the monitored command dumped core\nSegmentation fault\n", .expected_exit_code = 139});
}

TEST(Build, TakeFromInfiniteList) {
  test_build("type 'a list = | Null | Cons of 'a * 'a list;;\n"
             "let rec print_list l = match l with\n"
             "| Null -> ()\n"
             "| Cons (x,xs) -> print_int x; print_list xs;;\n"
             "let rec take l n = if n=0 then Null else match l with\n"
             "| Null -> Null\n"
             "| Cons (x,xs) -> Cons (x,take xs (n-1))\n;;"
             "let rec a = Cons(1,a);;\n"
             " print_list (take a 4) ;;\n"
             "let rec b = Cons(2,c) and c = Cons(3,b);;\n"
             " print_list (take b 8) ;;", {.expected_stdout =  "1 1 1 1 2 3 2 3 2 3 2 3 "});

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
      "print_int lam; print_int mu;;\n", {.expected_stdout = "6 5 "});
}

TEST(Build, TortoiseAndHare_Simple) {
  test_build(
      floyd_algo
      "type 'a list = | Null | Cons of 'a * 'a list;;\n"

      "let list_examine_cycle l = \n"
      " let (lam,mu) = floyd (fun Cons(_,xs) -> xs) l in\n"
      "print_int lam; print_int mu;;\n"

      "let rec a = Cons(10,b) and b = Cons(20,a) and c = Cons (1 , Cons (2, Cons (3, Cons(4,a)) ) );;\n"

      "list_examine_cycle c;;", {.expected_stdout = "2 4 "});
}

TEST(Build, SmallComparison) {
  test_build(R"(
 print_int (3 < 4);;
    print_int (3 < 3);;
    println_int (3 < 2);;
)", {.expected_stdout = "1 0 0\n"});
}

TEST(Build, IntComparison) {
  test_build(R"(
    print_int (3 < 4);;
    print_int (3 < 3);;
    println_int (3 < 2);;

    print_int (0 < (-1));;
    print_int (0 < 0);;
    println_int (0 < 1);;

    print_int ((-1) < (-2));;
    print_int ((-1) < (-1));;
    println_int ((-1) <  0);;

    println_int ((-54) < (-53));;

)", {.expected_stdout = "1 0 0\n0 0 1\n0 0 1\n1\n"});
}

TEST(Build, Stream) {
  test_build(R"(
  type 'a seq = | Item of 'a * (unit -> 'a seq);;
  let rec print_int_stream n (Item (x,xf)) =
    if n=0
    then (
      println_int x
    ) else (
      print_int x; print_int_stream (n-1) (xf())
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

                      )", {.expected_stdout =
  "42 43 44 45 46 47 48 49 50 51 52\n"
  "85 87 89 91 93 95 97 99 101 103 105\n"
  "51 52 53 54 55 56 57 58 59 60 61\n"
  "1 3 6 10 15 21 28 36 45 55 66\n"});
}

TEST(Build, ArithmeticExpression) {
  test_build("println_int (4*5+27/34*32-27*34/32+5*(6+23)/45);;", {.expected_stdout = "-5\n"});
}

TEST(Build, InverseDigits) {
  test_build("let big_inverse x = 1000000 / x;;"
             "println_int (big_inverse 7);;", {.expected_stdout = "142857\n"});
}

TEST(Build, Multiplied) {
  test_build("let big_mul x =  1234567890 * x;;"
             "println_int (big_mul 7);;", {.expected_stdout = "8641975230\n"});
}

TEST(Build, CallWithDad) {
  test_build(R"(
type 'a lista = | Vuota | NonVuota of 'a * 'a lista ;;
let rec repeat x n = if n=0 then Vuota else (NonVuota (x, repeat x (n-1) ) );;
let rec print_list l = match l with | Vuota -> () | (NonVuota (x,rest)) -> (print_int x; print_list rest) ;;
print_list (repeat 42 10);;
)", {.expected_stdout = "42 42 42 42 42 42 42 42 42 42 "});
}

TEST(Build, BoxedTypeDestruction) {
  test_build(R"(
type 'a boxed = | Boxed of 'a ;;

let f x =
  let x = (Boxed x) ~> (fun (Boxed n) -> println_int n) in
  println_int 100;;

f 42;;
)", {.expected_stdout = "42\n100\n"});
  test_build(R"(
type 'a boxed = | Boxed of 'a ;;

let f x =
  let x = (Boxed x) ~> (fun (Boxed n) -> println_int n) in
  println_int 100; x;;

f 42;;
)", {.expected_stdout = "100\n42\n"});
  test_build(R"(
type 'a boxed = | Boxed of 'a ;;

let f x =
  let x = (Boxed x) ~> (fun (Boxed n) -> println_int n) in
  println_int 100; x;;

let apply2 (f1,f2) (x1,x2) = (f1 x1, f2 x2) ;;
let const x y = x;;

apply2 (const (), println_int) (f 42,34);;
(* First, the call to f1 print 100 and returns (Boxed 42);
    Then, in apply 2 the Boxed gets ignored by const () and hence gets destroyed;
    Finally 34 is printed (tuples are evaluated left to right) *)
)", {.expected_stdout = "100\n42\n34\n"});
  test_build(R"(
type 'a boxed = | Boxed of 'a ;;

let make_deadspeaking_box x = (Boxed x) ~> (fun (Boxed n) -> println_int n);;

type 'a list = | Empty | Cons of 'a * 'a list ;;

let rec repeat_ x n tail = if n=0 then tail else repeat_ x (n-1) (Cons (x,tail));;
let repeat x n = repeat_ x n Empty;;

repeat (repeat (make_deadspeaking_box 1729) 1000) 1000;;
(* Even by making "copies" of a value, these are functional copies, so the box is only deleted once*)
)", {.expected_stdout = "1729\n"});
  test_build(R"(
type 'a boxed = | Boxed of 'a ;;

let make_deadspeaking_box x = (Boxed x) ~> (fun (Boxed n) -> println_int n);;

deep_copy (make_deadspeaking_box 1729);;
)", {.expected_stdout = "1729\n1729\n"});
  test_build(R"(
type 'a boxed = | Boxed of 'a ;;

let make_deadspeaking_box x = (Boxed x) ~> (fun (Boxed n) -> println_int n);;

type 'a list = | Empty | Cons of 'a * 'a list ;;

let rec repeat_ x n tail = if n=0 then tail else repeat_ x (n-1) (Cons ((deep_copy x),tail));;
let repeat x n = repeat_ x n Empty;;

repeat (make_deadspeaking_box 1729) 5;;
)", {.expected_stdout = "1729\n1729\n1729\n1729\n1729\n1729\n"});

}

TEST(Build, StringLiteral) {
  test_build(R"(
(* ITER *)
let rec iter_ f i n = if i=n then () else f i; iter_ f (i+1) n;;
let iter f n = iter_ f 0 n;;

(* PRINT_STRING *)
let print_string s = iter (fun i -> print_chr (str_at s i)) (strlen s);;

print_string "Hello, \t World!\n" ;;

(* or, we can use the more efficient,
   already implemented, print_str   *)

print_str "Good morning, Mars!!!\n" ;;

(* also on other files *)

fprint_str stdout "This is on STDOUT!!!\n" ;;
fprint_str stderr "This is on STDERR!!!\n" ;;

)",
             {.expected_stdout="Hello, \t World!\nGood morning, Mars!!!\nThis is on STDOUT!!!\n",
                 .expected_stderr="This is on STDERR!!!\n"});
}

TEST(Build, WritingFile) {
  test_build(R"(

let fd = fopen "log.txt" "w" ;;
fprint_str fd "Hello, logging!\n" ;;
fclose fd ;;

)", {});
}

TEST(Build, AutoCloseFile) {
  static constexpr std::string_view source = R"(
(* First, define the library for the autoclosefile.
    We don't have modules yet, so we'll just prepend
  acf_ to the functions of this "module" for clarity.*)

(* Module Auto_close_file *)

(* These two should be not exposed *)
type acf_t = | Fd of int;;
let acf_fd (Fd fd) = fd;;

let acf_open path mode = (Fd (fopen path mode)) ~> (fun (Fd fd) -> fclose fd);;
let acf_open_msg path mode msg = (Fd (fopen path mode)) ~> (fun (Fd fd) -> fprint_str fd msg; fclose fd);;
let acf_print_str acf str = fprint_str (acf_fd acf) str; acf;;
let acf_print_int acf n = fprintln_int (acf_fd acf) n; acf;;
type acf_poly_t = | Int of int | Str of string ;;
let rec acf_print_poly acf x =
  (match x with
    | (Int n) -> fprintln_int (acf_fd acf) n
    | (Str s) -> fprint_str (acf_fd acf) s);
  acf_print_poly acf ;;
(* end module *)

let test () =
  let f = acf_open_msg "/tmp/log.txt" "w+" "##### Closing log file #####\n" in
  acf_print_str f "##### Opening log file #####\n"
  ; acf_print_poly f (Int 34)  (Str "Hello\n") (Int 62875)
;;

test ();;
test () (Int 1729) (Str "As long as we use it, the file is still alive!\n");;

)";
  test_build(source, {});
}

TEST(Build, Time) {
  static constexpr std::string_view source = R"(

  print_time (now ());;

)";
  time_t t = time(NULL);
  std::string exp(ctime(&t));
  exp[exp.size() - 8] = exp[exp.size() - 7] = '.';
  test_build(source, {.use_release_lib=true, .expected_stdout = ::testing::MatchesRegex(exp)});
}

/*

*/
//Test IDEA: let rec x = Some y and y = f ();;
