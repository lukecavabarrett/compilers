#include <gtest/gtest.h>
#include <parse/parse.h>
#include <util/message.h>
#include <build/build.h>

TEST(BuildPiecewise,Expression){
  constexpr std::string_view source = "int_print (int_sum 4 5)";
  parse::tokenizer tk(source);
  auto e = ast::expression::parse(tk);
  EXPECT_EQ(e->to_sexp_string(),"(ast::expression::fun_app (ast::expression::identifier int_print) (ast::expression::fun_app (ast::expression::fun_app (ast::expression::identifier int_sum) (ast::expression::literal (ast::literal::integer 4))) (ast::expression::literal (ast::literal::integer 5))))");
  auto fv = e->free_vars();
  EXPECT_EQ(fv.size(),2);
}

TEST(Build,Expression){
  constexpr std::string_view source = //"let () = int_print (int_sum 4 5) ;;\n"
                                      //"let rec a = Cons a;;\n"
                                      //"let loop x = loop x ;;\n"
                                      "let answer = 42 ;;\n"
                                      "let another_answer = answer;;\n"
                                      "let answer = 56\n"
                                      "and yet_another_answer = answer;;\n"
                                      "let a_pair = (92, 54) ;;\n"
                                      "let (x,y) = a_pair;;\n"
                                      "let sum_100 = int_sum 100 ;;\n"
                                      "let () = int_print (sum_100 (if (int_eq 107 106) then x else y)) ;;\n";
  build(source);
}