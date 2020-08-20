

#include <string_view>
#include <parse.h>
#include <iostream>
#include <fstream>
#include <string>
#include <fstream>
#include <streambuf>

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

std::string file_to_string(const char* path){
  std::ifstream t(path);
  return std::string((std::istreambuf_iterator<char>(t)),
                  std::istreambuf_iterator<char>());
}

void test1(){
  static constexpr std::string_view source = "Error _ , t ";
  auto tks = tokenize(source);
  auto [Ast,end] = ast::parse_matcher(tks.begin(),tks.end());
  assert(end==tks.end());
}

void test2(){
  static constexpr std::string_view source = "Upper lower";
  auto tks = tokenize(source);
  auto [Ast,end] = ast::parse_expression(tks.begin(),tks.end());
  assert(end==tks.end());
  assert(dynamic_cast<ast::fun_app*>(Ast.get()));
}

void test3(){
  static constexpr std::string_view source = "lower lower";
  auto tks = tokenize(source);
  auto [Ast,end] = ast::parse_expression(tks.begin(),tks.end());
  assert(end==tks.end());
  assert(dynamic_cast<ast::fun_app*>(Ast.get()));
}

void test4(){
  static constexpr std::string_view source = "let rec f () = x and x = f () ;;";
  auto tks = tokenize(source);
  auto [Ast,end] = ast::parse_definition(tks.begin(),tks.end());
}

int main() {
  test1();
  test2();
  test3();
  test4();
  auto source = file_to_string("/home/luke/CLionProjects/compilers/bml/lib/sample.ml");
  auto tks = tokenize(source);
  for (const token& t : tks)std::cout << "[" << t.to_string() << "] " ;
  std::cout<<std::endl;
  auto [Ast,end] = ast::parse_definition(tks.begin(),tks.end());
  //assert(end==tks.end());
  std::ofstream("out.html", std::ofstream::out | std::ofstream::trunc)<< Ast->to_html() << std::endl;
}