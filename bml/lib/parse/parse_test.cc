#include <gtest/gtest.h>
#include <parse/parse.h>
#include <util/message.h>

namespace {

TEST(Parse, Matcher1) {
  static constexpr std::string_view source = "Error _ , t ";
  auto tks = parse::tokenizer(source);
  auto ast = ast::matcher::parse(tks);
  EXPECT_TRUE(tks.empty());
}

template<auto ParseFun>
void parse_retrow(std::string_view source,std::string_view expected) {
  auto tks = parse::tokenizer( source );
  try {
    auto ast = ParseFun(tks);
    EXPECT_TRUE(tks.empty());
    EXPECT_EQ(ast->to_sexp_string(),expected);
  } catch (const util::error::message& m) {
    m.print(std::cout, source, "source");
    FAIL();
  }
}

#define MACROS_CONCAT_NAME_INNER(x, y) x##y
#define MACROS_CONCAT_NAME(x, y) MACROS_CONCAT_NAME_INNER(x, y)
#define TEST_PARSE_NS_IMPL(ns, test_name, source, expected) TEST ( \
  Parse, test_name ) {                                             \
  parse_retrow< ast:: ns ::parse >(source,expected);                  \
}

#define TEST_PARSE_EXPRESSION(source, expected) TEST_PARSE_NS_IMPL( \
  expression, MACROS_CONCAT_NAME(Expression, __COUNTER__), source,expected)
#define TEST_PARSE_MATCHER(source, expected) TEST_PARSE_NS_IMPL( \
  matcher, MACROS_CONCAT_NAME(Matcher, __COUNTER__), source,expected)
#define TEST_PARSE_DEFINITION(source, expected) TEST_PARSE_NS_IMPL( \
  definition , MACROS_CONCAT_NAME(Definition, __COUNTER__), source,expected)
#define TEST_PARSE_TYPE(source, expected) TEST_PARSE_NS_IMPL( \
  type::expression, MACROS_CONCAT_NAME(Type, __COUNTER__), source,expected)
#define TEST_PARSE_TYPE_DECL(source, expected) TEST_PARSE_NS_IMPL( \
  type::definition, MACROS_CONCAT_NAME(TypeDef, __COUNTER__), source,expected)


TEST_PARSE_EXPRESSION("Upper lower", "(ast::expression::fun_app (ast::expression::constructor Upper) (ast::expression::identifier lower))");
TEST_PARSE_EXPRESSION("lower lower", "(ast::expression::fun_app (ast::expression::identifier lower) (ast::expression::identifier lower))");

TEST_PARSE_MATCHER("Error _ , t ","(ast::matcher::tuple_matcher ((ast::matcher::constructor_matcher Error (ast::matcher::anonymous_universal_matcher)) (ast::matcher::universal_matcher t)))")
TEST_PARSE_EXPRESSION("match t with\n"
                      "| Some value -> Some (f value)\n"
                      "| None -> None", "(ast::expression::match_with (ast::expression::identifier t) ((ast::expression::match_with::branch (ast::matcher::constructor_matcher Some (ast::matcher::universal_matcher value)) (ast::expression::fun_app (ast::expression::constructor Some) (ast::expression::fun_app (ast::expression::identifier f) (ast::expression::identifier value)))) (ast::expression::match_with::branch (ast::matcher::constructor_matcher None NULL) (ast::expression::constructor None))))");
TEST_PARSE_DEFINITION("let rec f x = f x and x = f () ",
                      "(ast::definition::t 1 ((ast::definition::function (ast::matcher::universal_matcher f) ((ast::matcher::universal_matcher x)) (ast::expression::fun_app (ast::expression::identifier f) (ast::expression::identifier x))) (ast::definition::value (ast::matcher::universal_matcher x) (ast::expression::fun_app (ast::expression::identifier f) (ast::expression::literal (ast::literal::unit ()))))))");

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
