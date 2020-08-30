#include <gtest/gtest.h>
#include <parse/parse.h>

namespace {

TEST(Parse, Matcher1) {
  static constexpr std::string_view source = "Error _ , t ";
  auto tks = parse::tokenizer(source);
  auto ast = ast::matcher::parse(tks);
  EXPECT_TRUE(tks.empty());
}

TEST(Parse, Expression1) {
  static constexpr std::string_view source = "Upper lower";
  auto tks = parse::tokenizer(source);
  ast::expression::ptr ast;
  EXPECT_NO_THROW(ast = ast::expression::parse(tks));
  EXPECT_TRUE(tks.empty());
  EXPECT_EQ(ast->to_sexp_string(), "(ast::expression::fun_app (ast::expression::identifier) (ast::expression::identifier))");
}

TEST(Parse, Expression2) {
  static constexpr std::string_view source = "lower lower";
  auto tks = parse::tokenizer(source);
  ast::expression::ptr ast;
  EXPECT_NO_THROW(ast = ast::expression::parse(tks));
  EXPECT_TRUE(tks.empty());
  EXPECT_EQ(ast->to_sexp_string(), "(ast::expression::fun_app (ast::expression::identifier) (ast::expression::identifier))");
}

TEST(Parse, Definition) {
  static constexpr std::string_view source = "let rec f x = f x and x = f () ;;";
  auto tks = parse::tokenizer(source);
  ast::definition::ptr ast;
  EXPECT_NO_THROW(ast = ast::definition::parse(tks));
  EXPECT_NO_THROW(tks.expect_pop(parse::EOC));
  EXPECT_TRUE(tks.empty());
}

#define MACROS_CONCAT_NAME_INNER(x, y) x##y
#define MACROS_CONCAT_NAME(x, y) MACROS_CONCAT_NAME_INNER(x, y)
#define TEST_PARSE_NS_IMPL(ns, test_name, source, expected) TEST ( \
  Parse, test_name ) {                                \
  auto tks = parse::tokenizer( source );                        \
ast:: ns ::ptr ast ;\
  EXPECT_NO_THROW( \
  ast = ast:: ns ::parse(tks) );\
  EXPECT_TRUE(\
  tks.empty());                                                 \
  EXPECT_EQ(ast->to_sexp_string(),expected);                    \
}

#define TEST_PARSE_TYPE(source, expected) TEST_PARSE_NS_IMPL( \
  type::expression, MACROS_CONCAT_NAME(Type, __COUNTER__), source,expected)
#define TEST_PARSE_TYPE_DECL(source, expected) TEST_PARSE_NS_IMPL( \
  type::definition, MACROS_CONCAT_NAME(TypeDef, __COUNTER__), source,expected)

TEST_PARSE_TYPE("int", "(ast::type::expression::identifier int)");
TEST_PARSE_TYPE("int list", "(ast::type::expression::constr (ast::type::expression::identifier int) (ast::type::expression::identifier list))");
TEST_PARSE_TYPE("int option", "(ast::type::expression::constr (ast::type::expression::identifier int) (ast::type::expression::identifier option))");
TEST_PARSE_TYPE("int option list", "(ast::type::expression::constr (ast::type::expression::constr (ast::type::expression::identifier int) (ast::type::expression::identifier option)) (ast::type::expression::identifier list))");
TEST_PARSE_TYPE("int list option list",
                "(ast::type::expression::constr (ast::type::expression::constr (ast::type::expression::constr (ast::type::expression::identifier int) (ast::type::expression::identifier list)) (ast::type::expression::identifier option)) (ast::type::expression::identifier list))");
TEST_PARSE_TYPE("(int,bool) result", "(ast::type::expression::constr (ast::type::expression::tuple ((ast::type::expression::identifier int) (ast::type::expression::identifier bool))) (ast::type::expression::identifier result))");
TEST_PARSE_TYPE("(int * bool) list", "(ast::type::expression::constr (ast::type::expression::product ((ast::type::expression::identifier int) (ast::type::expression::identifier bool))) (ast::type::expression::identifier list))"
);
TEST_PARSE_TYPE("int * bool list", "(ast::type::expression::product ((ast::type::expression::identifier int) (ast::type::expression::constr (ast::type::expression::identifier bool) (ast::type::expression::identifier list))))");
TEST_PARSE_TYPE("int * (bool list)", "(ast::type::expression::product ((ast::type::expression::identifier int) (ast::type::expression::constr (ast::type::expression::identifier bool) (ast::type::expression::identifier list))))"
);
TEST_PARSE_TYPE("'a list -> int", "(ast::type::expression::function (ast::type::expression::constr (ast::type::expression::identifier 'a) (ast::type::expression::identifier list)) (ast::type::expression::identifier int))"
);
TEST_PARSE_TYPE("'a list -> ('a -> 'a -> bool) -> unit -> 'a list",
                "(ast::type::expression::function (ast::type::expression::constr (ast::type::expression::identifier 'a) (ast::type::expression::identifier list)) (ast::type::expression::function (ast::type::expression::function (ast::type::expression::identifier 'a) (ast::type::expression::function (ast::type::expression::identifier 'a) (ast::type::expression::identifier bool))) (ast::type::expression::function (ast::type::expression::identifier unit) (ast::type::expression::constr (ast::type::expression::identifier 'a) (ast::type::expression::identifier list)))))"
);
TEST_PARSE_TYPE_DECL("type block = int * t and t = unit -> block option",
                     "(ast::type::definition::t 0 ((ast::type::definition::single_texpr block (ast::type::expression::product ((ast::type::expression::identifier int) (ast::type::expression::identifier t)))) (ast::type::definition::single_texpr t (ast::type::expression::function (ast::type::expression::identifier unit) (ast::type::expression::constr (ast::type::expression::identifier block) (ast::type::expression::identifier option))))))"
);
TEST_PARSE_TYPE_DECL("type 'a block = 'a * 'a t and 'a t = unit -> 'a block option",
                     "(ast::type::definition::t 0 ((ast::type::definition::single_texpr block (ast::type::expression::product ((ast::type::expression::identifier 'a) (ast::type::expression::constr (ast::type::expression::identifier 'a) (ast::type::expression::identifier t))))) (ast::type::definition::single_texpr t (ast::type::expression::function (ast::type::expression::identifier unit) (ast::type::expression::constr (ast::type::expression::constr (ast::type::expression::identifier 'a) (ast::type::expression::identifier block)) (ast::type::expression::identifier option))))))"
);
TEST_PARSE_TYPE_DECL("type 'a or_error = ('a,error) result ",
                     "(ast::type::definition::t 0 ((ast::type::definition::single_texpr or_error (ast::type::expression::constr (ast::type::expression::tuple ((ast::type::expression::identifier 'a) (ast::type::expression::identifier error))) (ast::type::expression::identifier result)))))"
);
TEST_PARSE_TYPE_DECL("type ('a,'b) tree = ('a,'b) result ",
                     "(ast::type::definition::t 0 ((ast::type::definition::single_texpr tree (ast::type::expression::constr (ast::type::expression::tuple ((ast::type::expression::identifier 'a) (ast::type::expression::identifier 'b))) (ast::type::expression::identifier result)))))"
);
TEST_PARSE_TYPE_DECL("type ('a,'b) result = | Okay of 'a | Error of 'b ",
                     "(ast::type::definition::t 0 ((ast::type::definition::single_variant result ((ast::type::definition::single_variant::constr Okay (ast::type::expression::identifier 'a)) (ast::type::definition::single_variant::constr Error (ast::type::expression::identifier 'b))))))"
);

}