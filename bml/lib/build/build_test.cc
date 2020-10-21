#include <gtest/gtest.h>
#include <parse/parse.h>
#include <util/message.h>
#include <build/build.h>
#define target  "/home/luke/CLionProjects/compilers/bml/output"

std::string load_file(const char *path) {
  std::ifstream f(path);
  return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

void test_build(std::string_view source, std::string_view expected_stdout, int expected_exit_code = 0, std::string_view expected_stderr = "") {
  std::ofstream oasm;
  oasm.open(target ".asm");
  ASSERT_NO_THROW(build(source, oasm));
  oasm.close();

  ASSERT_EQ(system("yasm -g dwarf2 -f elf64 " target ".asm -l " target ".lst -o " target ".o"), 0);
  ASSERT_EQ(system("gcc -no-pie " target ".o -o " target), 0);
  int exit_code = WEXITSTATUS(system("timeout 1 " target " 2> " target ".stderr 1> " target ".stdout"));
  EXPECT_EQ(load_file(target ".stdout"), expected_stdout);
  EXPECT_EQ(load_file(target ".stderr"), expected_stderr);
  EXPECT_EQ(exit_code, expected_exit_code);
}

TEST(Build, Expression0) {
  constexpr std::string_view source = "let answer = 42;;"
                                      "let () = int_print 42;;\n";
  test_build(source, "42\n");

}

TEST(Build, Expression1) {
  constexpr std::string_view source = "let answer = 42 ;;\n"
                                      "let another_answer = answer;;\n"
                                      "let answer = 56\n"
                                      "and yet_another_answer = answer;;\n"
                                      "let a_pair = (92, 54) ;;\n"
                                      "let (x,y) = a_pair;;\n"
                                      "let sum_100 = int_sum 100 ;;\n"
                                      "let () = int_print (sum_100 (if (int_eq 107 106) then x else y)) ;;\n";
  test_build(source, "154\n");

}

TEST(Build, Expression2) {
  constexpr std::string_view source = "let g f x = f x 1 ;;\n"
                                      "let y = g int_sum 10;;\n"
                                      "let () = int_print y;;";
  test_build(source, "11\n");
}

TEST(Build, Expression3) {
  constexpr std::string_view source = "let twice x = x + x ;;\n"
                                      "let ans = 45 + 25 - 93 + twice (97 + 21);;\n"
                                      "let () = int_print ans;;";
  test_build(source, "213\n");
}

TEST(Build, Expression4) {
  test_build("let f x () = int_print x ;;\n"
             "let g = f 42;;\n"
             "let () = g ();;", "42\n");
}

TEST(Build, Constructor) {
  test_build("type int_option = | None | Some of int ;;\n"
             "let some13 = Some 13 and none = None;;\n"
             "let Some 13 = some13;;\n"
             "let Some x = some13;;\n"
             "let () = int_print x;;\n", "13\n");
}

TEST(Build, SomeAddition) {
  test_build("type int_option = | Some of int ;;\n"
             "let some_add (Some x) (Some y) (Some z) = Some (x+y+z);;\n"
             "let Some ans = some_add (Some 10) (Some 100) (Some 1);;\n"
             "let () = int_print ans;;\n", "111\n");
}

TEST(Build, OptionMapSome) {
  test_build("type int_option = | None | Some of int ;;\n"
             "let option_map f xo = match xo with | None -> None | Some x -> Some (f x);;\n"
             "let _ = option_map int_print (Some 42);;\n", "42\n");

}

TEST(Build, OptionMapNone) {
  test_build("type int_option = | None | Some of int ;;\n"
             "let option_map f xo = match xo with | None -> None | Some x -> Some (f x);;\n"
             "let _ = option_map int_print None;;\n", "");
}

TEST(Build, OptionMapError) {
  test_build("type int_option = | None | Some of int | Another ;;\n"
             "let option_map f xo = match xo with | None -> None | Some x -> Some (f x);;\n"
             "let _ = option_map int_print Another;;\n", "", 1, "match failed\n");
}

TEST(Build, ApplyTwice) {
  test_build("let apply_twice f x = f (f x);;\n"
             "let plus_two = apply_twice (fun x -> x + 1);;\n"
             "let () = int_print (plus_two 40);;\n", "42\n");
}

TEST(Build, ApplyTwiceOnSteroids) {
  test_build("let apply_twice f x = f (f x);;\n"
             "let () = int_print (apply_twice apply_twice (fun x -> x + 1) 0);;\n", "4\n");
}

TEST(Build, FnCompose) {
  test_build("let fn_compose g f x = g (f x);;\n"
             "let () = int_print (fn_compose (fun x -> x+x) (fun x -> x + 1) 12);;\n", "26\n");
}

TEST(Build, OptionMap) {
  test_build("type int_option = | None | Some of int ;;\n"
             "let option_map f x = match x with | None -> None | Some x -> Some (f x);;\n"
             "let maybe_add y x = match y with | None -> None | Some y -> Some (x+y);;"
             "let Some ans = maybe_add (Some 10) 1;;\n"
             "let () = int_print ans;;\n", "11\n");
}

TEST(Build, MaybeAdditionWrong) {
  test_build("type int_option = | None | Some of int ;;\n"
             "let option_map f x = match x with | None -> None | Some x -> Some (f x);;\n"
             "let maybe_add y x = match y with | None -> None | Some y -> Some (x+y);;\n"
             "let maybe_sum x y = option_map (maybe_add y) x;;\n"
             "let Some (Some ans) = maybe_sum (Some 10) (Some 100);;\n"
             "let () = int_print ans;;\n", "110\n");
}

TEST(Build, MaybeAdditionCorrect) {
  test_build("type int_option = | None | Some of int ;;\n"
             "let option_map f x = match x with | None -> None | Some x -> Some (f x);;\n"
             "let option_bind f x = match x with | None -> None | Some x -> f x;;\n"
             "let maybe_add y x = match y with | None -> None | Some y -> Some (x+y);;\n"
             "let maybe_sum x y = option_bind (maybe_add y) x;;\n"
             "let maybe_print x = match x with | None -> int_print (0-1) | Some x -> int_print x;;\n"
             "let _ = maybe_print (maybe_sum (Some 10) (Some 100));;\n"
             "let _ = maybe_print (maybe_sum (None) (Some 100));;\n"
             "let _ = maybe_print (maybe_sum (Some 10) (None));;\n"
             "let _ = maybe_print (maybe_sum (None) (None));;\n", "110\n-1\n-1\n-1\n");
}

TEST(Build, NonGlobalFunction) {
  test_build("let play_with x = int_print x; (let y = x + x in y);;\n"
             "let ans = play_with 10;;"
             "let () = int_print ans;;", "10\n20\n");
}

TEST(Build, Malloc) {
  test_build("type option = | Some of int;;\n"
             "let heap_big_tuple = Some (1,2,3,4,5,6,7);;\n"
             "let stck_big_tuple = 1,2,3,4,5,6,7;;", "");
}

TEST(Build, CaptureX) {
  test_build("let plus_x x = (fun y -> x + y);;\n"
             "let plus_3 = plus_x 3;;\n"
             "let () = int_print (plus_3 4);;", "7\n");
}

TEST(Build, CaptureMaybeSum) {
  test_build("type int_option = | None | Some of int ;;\n"
             "let option_map f x = match x with | None -> None | Some x -> Some (f x);;\n"
             "let option_bind f x = match x with | None -> None | Some x -> f x;;\n"
             "let maybe_sum y x = option_bind (fun x -> option_map (fun y -> x + y) y) x;;\n"
             "let maybe_print x = match x with | None -> int_print (0-1) | Some x -> int_print x;;\n"
             "let _ = maybe_print (maybe_sum (Some 10) (Some 100));;\n"
             "let _ = maybe_print (maybe_sum (None) (Some 100));;\n"
             "let _ = maybe_print (maybe_sum (Some 10) (None));;\n"
             "let _ = maybe_print (maybe_sum (None) (None));;\n", "110\n-1\n-1\n-1\n");
}

TEST(Build, LongSum) {
  test_build("let long_sum a b c d e f g h i j k = a+b+c+d+e+f+g+h+i+j+k;;\n"
             "let ans = long_sum 1 2 3 4 5 6 7 8 9 10 11;;\n"
             "let () = int_print ans ;;", "66\n");
}

TEST(Build, DeepCapture) {
  test_build("let deep_capture x = fun () -> fun () -> fun () -> fun () -> fun () -> fun () -> fun () -> x ;;"
             "      let () = int_print (deep_capture 1729 () () () () () () ());;", "1729\n");
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
             "let () = g Void;;\n"
             "let () = g (Int 0);;\n"
             "let () = g (Int 1);;\n"
             "let () = g (Int 64);;\n"
             "let () = g (Tuple (0,Int 3));;\n"
             "let () = g (Tuple (1000,Void));;\n"
             "let () = g (Tuple (1000,Int 100));;\n"
             "let () = g (Tuple (1000,Tuple(10000, Tuple(100,Void))));;\n"
             "let () = g (Tuple (1000,Void));;\n", "0\n1\n2\n3\n4\n1005\n1106\n11107\n1005\n");
}


TEST(Build, MatchersTheRevenge) {
  test_build("type t = | Null | Triple of (int * int) * int;;\n"
             "let f x = match x with\n"
             "| Null -> 0\n"
             "| Triple ((x,y),z) -> x + y + z;;\n"
             "let g x = int_print (f x);;\n"
             "let () = g Null;;\n"
             "let () = g (Triple ((1,10),100));;\n", "0\n111\n");
}


