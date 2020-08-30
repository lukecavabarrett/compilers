

#include <string_view>
#include <parse/parse.h>
#include <iostream>
#include <fstream>
#include <string>


constexpr std::string_view source_long =
    "let rec even n =\n"
    "    match n with\n"
    "    | 0 -> true\n"
    "    | n -> odd (int_sub n 1)\n"
    "and odd n =\n"
    "    match n with\n"
    "    | 0 -> false\n"
    "    | n -> even (int_sub n 1)\n"
    "in even 42 ;;";

constexpr std::string_view source_if_then_else =
    "if (equal x 43) then\n"
    "  do_something (); 37\n"
    "else\n"
    "  make_failure x ;;";



void test1(){
  static constexpr std::string_view source = "Error _ , t ";
  auto tks = parse::tokenizer(source);
  auto ast = ast::matcher::parse(tks);
  assert(tks.empty());
}

void test2(){
  static constexpr std::string_view source = "Upper lower";
  auto tks = parse::tokenizer(source);
  auto ast = ast::expression::parse(tks);
  assert(tks.empty());
  assert(dynamic_cast<ast::expression::fun_app*>(ast.get()));
}

void test3(){
  static constexpr std::string_view source = "lower lower";
  auto tks = parse::tokenizer(source);
  auto ast = ast::expression::parse(tks);
  assert(tks.empty());
  assert(dynamic_cast<ast::expression::fun_app*>(ast.get()));
}

void test4(){
  static constexpr std::string_view source = "let rec f x = f x in f () ;;";
  auto tks = parse::tokenizer(source);
  auto ast = ast::expression::parse(tks);
  tks.expect_pop(parse::EOC);
  assert(tks.empty());
  ast->bind(ast::ltable());
}

int main() {
  test1();
  test2();
  test3();
  test4();
  std::string_view path = ("/home/luke/CLionProjects/compilers/bml/lib/sample.ml");

  ast::compile(load_file(path),ast::ltable(),path);

  /*auto tks = parse::tokenizer(source);
  std::cout<<std::endl;
  auto ast = ast::expression::parse(tks);
  std::ofstream("out.html", std::ofstream::out | std::ofstream::trunc)<< ast->to_html() << std::endl;
  ast->bind(ast::ltable());*/

}