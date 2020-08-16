

#include <string_view>
#include <parse.h>
#include <iostream>

constexpr std::string_view source =
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
    "if (equal x 43) then do_something (); 37 else make_failure x ;;";

int main() {
  auto tks = tokenize(source_if_then_else);
  for (const token& t : tks)std::cout << "[" << t.to_string() << "] " ;
  std::cout<<std::endl;
  auto [Ast,end] = ast::parse_expression(tks.begin(),tks.end());
  assert(end==tks.end()-1);
  std::cout << Ast->to_html() << std::endl;
}