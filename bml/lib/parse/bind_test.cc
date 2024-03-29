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
  EXPECT_EQ(util::texp::make_texp(fv1)->to_string(), "{}");;
  fv1 = ast->free_vars();
  EXPECT_EQ(util::texp::make_texp(fv1)->to_string(), "{}");
}

TEST(Bind, Expression2) {
  static constexpr std::string_view source = "let rec even n =\n"
                                             "match n with\n"
                                             "| 0 -> true\n"
                                             "| _ -> odd (n - 1)\n"
                                             "and odd n =\n"
                                             "match n with \n"
                                             "| 0 -> false\n"
                                             "| _ -> even (n - 1)";
  auto tks = parse::tokenizer(source);
  auto ast = ast::definition::parse(tks);
  auto fv1 = ast->free_vars();
  EXPECT_EQ(util::texp::make_texp(fv1)->to_string(), "{'__binary_op__MINUS__' : [ast::expression::identifier{name : '__binary_op__MINUS__'}, ast::expression::identifier{name : '__binary_op__MINUS__'}]}");
}

TEST(Bind, Expression3) {
  static constexpr std::string_view source = "let rec fold f l init = match l with\n"
                                             "| Empty -> init\n"
                                             "| Cons (x,xs) -> fold f xs (f init x)";
  auto tks = parse::tokenizer(source);
  auto ast = ast::definition::parse(tks);
  auto fv1 = ast->free_vars();
  EXPECT_EQ(util::texp::make_texp(fv1)->to_string(), "{}");
  auto cs = ast->capture_group();
  EXPECT_EQ(util::texp::make_texp(cs)->to_string(), "{}");

}

TEST(Bind, Expression4) {
  static constexpr std::string_view source = "let apply_double f =\n"
                                             "let g x = f (f x) in\n"
                                             "g";
  auto tks = parse::tokenizer(source);
  auto ast = ast::definition::parse(tks);
  auto fv1 = ast->free_vars();
  EXPECT_EQ(util::texp::make_texp(fv1)->to_string(), "{}");
}

TEST(Bind, FreeVars) {
  static constexpr std::string_view source = "Some (x+y+z);;";
  auto tks = parse::tokenizer(source);
  auto ast = ast::expression::parse(tks);
  auto fv = ast->free_vars();
  EXPECT_EQ(fv.size(), 4);
}

TEST(Bind, MatchWhatAfterVars) {
  static constexpr std::string_view source = "match f x with\n"
                                             "| Add (y,z) -> int_add y z\n"
                                             "| Sub (x,y) -> int_sub x y";
  auto tks = parse::tokenizer(source);
  auto ast = ast::expression::parse(tks);
  auto fv1 = ast->free_vars();
  EXPECT_EQ(util::texp::make_texp(fv1)->to_string(),
            "{'int_sub' : [ast::expression::identifier{name : 'int_sub'}], 'int_add' : [ast::expression::identifier{name : 'int_add'}], 'f' : [ast::expression::identifier{name : 'f'}], 'x' : [ast::expression::identifier{name : 'x'}]}");
  ast::matcher::universal glo("global definition");
  glo.top_level = true;
  for (auto&[_, l] : fv1)for (auto id : l)id->definition_point = &glo;
  auto cs = ast->capture_group();
  EXPECT_EQ(util::texp::make_texp(cs)->to_string(), "{}");
}

TEST(Bind, RightCaptureSet) {
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
  const ast::definition::t &d = *dynamic_cast<const ast::expression::let_in *>(dynamic_cast<const ast::expression::fun *>(ast->defs.at(0).e.get())->body.get())->d;
  EXPECT_EQ(d.defs.size(), 2);
  const ast::expression::fun &even = *dynamic_cast<const ast::expression::fun *>(d.defs.at(0).e.get());
  const ast::expression::fun &odd = *dynamic_cast<const ast::expression::fun *>(d.defs.at(1).e.get());
  EXPECT_EQ(util::texp::make_texp(even.captures)->to_string(), "[ast::matcher::universal{name : 'is_zero'}, ast::matcher::universal{name : 'prev'}, ast::matcher::universal{name : 'odd'}]");
  EXPECT_EQ(util::texp::make_texp(odd.captures)->to_string(), "[ast::matcher::universal{name : 'is_zero'}, ast::matcher::universal{name : 'prev'}, ast::matcher::universal{name : 'even'}]");
}

TEST(Bind, CaptureSet) {
  static constexpr std::string_view source = "let rec iota n = Item (n, (fun () -> iota (n+1)) );;";
  auto tks = parse::tokenizer(source);
  auto ast = ast::definition::parse(tks);
  auto fv = ast->free_vars();
  EXPECT_EQ(util::texp::make(fv)->to_string(),  "{'__binary_op__PLUS__' : [ast::expression::identifier{name : '__binary_op__PLUS__'}]}");
  ast::matcher::universal plus("+");
  plus.use_as_immediate = plus.top_level = true;
  fv.at("__binary_op__PLUS__").front()->definition_point = &plus;
  ast::matcher::universal& iota_name = *dynamic_cast<ast::matcher::universal*>(ast->defs.at(0).name.get());
  iota_name.top_level = iota_name.use_as_immediate = true;
  auto cs = ast->capture_group();
  EXPECT_TRUE(cs.empty());
  ast::expression::fun &iota = *dynamic_cast<ast::expression::fun *>(ast->defs.at(0).e.get());
  EXPECT_TRUE(iota.captures.empty());
  ast::expression::fun &inner = *dynamic_cast<ast::expression::fun *>(
      dynamic_cast<ast::expression::tuple *>(
          dynamic_cast<ast::expression::constructor *>(iota.body.get())->arg.get()
          )->args.at(1).get());
  EXPECT_EQ(inner.captures.size(),1);
}

}