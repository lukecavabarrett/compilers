#include <gtest/gtest.h>
#include <parse/parse.h>
#include <util/message.h>
#include <build/build.h>
#define target  "/home/luke/CLionProjects/compilers/bml/output"

void test_build(std::string_view source, std::string_view expected_output, int expected_exit_code = 0) {
  std::ofstream oasm;
  oasm.open(target ".asm");
  ASSERT_NO_THROW(build(source, oasm));
  oasm.close();

  ASSERT_EQ(system("yasm -g dwarf2 -f elf64 " target ".asm -l " target ".lst -o " target ".o"), 0);
  ASSERT_EQ(system("gcc -no-pie " target ".o -o " target), 0);
  FILE *fp = popen(target, "r");
  std::string output;
  std::array<char, 128> buffer;
  while (std::fgets(buffer.data(), 128, fp) != NULL) {
    output += buffer.data();
  }
  int exit_code = WEXITSTATUS(pclose(fp));
  EXPECT_EQ(output, expected_output);
  EXPECT_EQ(exit_code, expected_exit_code);
}

TEST(BuildPiecewise, Expression) {
  constexpr std::string_view source = "int_print (int_sum 4 5)";
  parse::tokenizer tk(source);
  auto e = ast::expression::parse(tk);
  EXPECT_EQ(e->to_sexp_string(),
            "(ast::expression::fun_app (ast::expression::identifier int_print) (ast::expression::fun_app (ast::expression::fun_app (ast::expression::identifier int_sum) (ast::expression::literal (ast::literal::integer 4))) (ast::expression::literal (ast::literal::integer 5))))");
  auto fv = e->free_vars();
  EXPECT_EQ(fv.size(), 2);
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
             "let _ = option_map int_print Another;;\n", "");
}

/*
TEST(Build, MaybeAddition) {
  test_build("type int_option = | None | Some of int ;;\n"
             "let option_bind f x = match x with | None -> None | Some x -> Some (f x);;\n"
             "let maybe_add y x = match y with | None -> None | Some y -> Some (x+y);;"
             "let maybe_sum x y = option_bind (maybe_add y) x;;\n"
             "let Some ans = maybe_sum (Some 10) (Some 100) (Some 1);;\n"
             "let () = int_print ans;;\n", "111\n");
}*/

