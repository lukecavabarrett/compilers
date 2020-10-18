#include <gtest/gtest.h>
#include <parse/parse.h>
#include <util/message.h>

namespace {

TEST(Bind, Expression1) {
  static constexpr std::string_view source = "let map t f =\n"
                                             "match t with\n"
                                             "| Some value -> Some (f value)\n"
                                             "| None -> None";
  auto tks = parse::tokenizer(source);
  auto ast = ast::definition::parse(tks);
  auto fv1 = ast->free_vars();
  EXPECT_EQ(util::sexp::make_sexp(fv1).to_string(),"()");;
  fv1 = ast->free_vars();
  EXPECT_EQ(util::sexp::make_sexp(fv1).to_string(),"()");
}

TEST(Bind, Expression2) {
  static constexpr std::string_view source = "let rec even n =\n"
                                             "match n with\n"
                                             "| 0 -> true\n"
                                             "| _ -> odd (int_sub n 1)\n"
                                             "and odd n =\n"
                                             "match n with \n"
                                             "| 0 -> false\n"
                                             "| _ -> even (int_sub n 1)";
  auto tks = parse::tokenizer(source);
  auto ast = ast::definition::parse(tks);
  auto fv1 = ast->free_vars();
  EXPECT_EQ(util::sexp::make_sexp(fv1).to_string(),"((int_sub ((ast::expression::identifier int_sub) (ast::expression::identifier int_sub))))");
}

TEST(Bind, Expression3) {
  static constexpr std::string_view source = "let rec fold f l init = match l with\n"
                                             "| Empty -> init\n"
                                             "| Cons (x,xs) -> fold f xs (f init x)";
  auto tks = parse::tokenizer(source);
  auto ast = ast::definition::parse(tks);
  auto fv1 = ast->free_vars();
  EXPECT_EQ(util::sexp::make_sexp(fv1).to_string(),"()");
  auto cs = ast->capture_group();
  EXPECT_EQ(util::sexp::make_sexp(cs).to_string(),"()");

}



TEST(Bind,Expression4){
  static constexpr std::string_view source = "let apply_double f =\n"
                                             "let g x = f (f x) in\n"
                                             "g";
  auto tks = parse::tokenizer(source);
  auto ast = ast::definition::parse(tks);
  auto fv1 = ast->free_vars();
  EXPECT_EQ(util::sexp::make_sexp(fv1).to_string(),"()");
}

TEST(Bind, FreeVars) {
  static constexpr std::string_view source = "Some (x+y+z);;";
  auto tks = parse::tokenizer(source);
  auto ast = ast::expression::parse(tks);
  auto fv = ast->free_vars();
  EXPECT_EQ(fv.size(),4);
}

TEST(Bind, MatchWhatAfterVars) {
  static constexpr std::string_view source = "match f x with\n"
                                             "| Add (y,z) -> int_add y z\n"
                                             "| Sub (x,y) -> int_sub x y";
  auto tks = parse::tokenizer(source);
  auto ast = ast::expression::parse(tks);
  auto fv1 = ast->free_vars();
  EXPECT_EQ(util::sexp::make_sexp(fv1).to_string(),"((int_sub ((ast::expression::identifier int_sub))) (int_add ((ast::expression::identifier int_add))) (f ((ast::expression::identifier f))) (x ((ast::expression::identifier x))))");
  ast::matcher::universal_matcher glo("global definition");
  glo.top_level = true;
  for(auto& [_,l] : fv1)for(auto id : l)id->definition_point = &glo;
  auto cs = ast->capture_group();
  EXPECT_EQ(util::sexp::make_sexp(cs).to_string(),"()");
}

TEST(Bind,RightCaptureSet){
  static constexpr std::string_view source = "let even_or_odd is_zero prev x =\n"
                                             "  let rec even x ="
                                             "    if is_zero x then Even else odd (prev x)"
                                             "  and odd x ="
                                             "    if is_zero x then Odd else even (prev x)"
                                             "  in"
                                             "  even x";
  auto tks = parse::tokenizer(source);
  auto ast = ast::definition::parse(tks);
  auto fv = ast->free_vars();
  EXPECT_TRUE(fv.empty());
  auto cs = ast->capture_group();
  EXPECT_TRUE(cs.empty());
  const ast::definition::t& d = * dynamic_cast<const ast::expression::let_in*>(dynamic_cast<const ast::definition::function*>(ast->defs.at(0).get())->body.get())->d;
  EXPECT_EQ(d.defs.size(),2);
  const ast::definition::function& even = *dynamic_cast<const ast::definition::function*>(d.defs.at(0).get());
  const ast::definition::function& odd = *dynamic_cast<const ast::definition::function*>(d.defs.at(1).get());
  EXPECT_EQ(even.name->name,"even");
  EXPECT_EQ(odd.name->name,"odd");
  EXPECT_EQ(util::sexp::make_sexp(even.captures).to_string(),"((ast::matcher::universal_matcher is_zero) (ast::matcher::universal_matcher prev) (ast::matcher::universal_matcher odd))");
  EXPECT_EQ(util::sexp::make_sexp(odd.captures).to_string(),"((ast::matcher::universal_matcher is_zero) (ast::matcher::universal_matcher prev) (ast::matcher::universal_matcher even))");
}


}