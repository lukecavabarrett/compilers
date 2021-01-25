#include <gtest/gtest.h>
#include <parse/parse.h>
#include <util/message.h>

namespace {

template<auto ParseFun>
void parse_expect_rethrow(std::string_view source, std::string_view expected) {
  try {
    auto tks = parse::tokenizer(source);
    auto ast = ParseFun(tks);
    EXPECT_TRUE(tks.empty());
    EXPECT_EQ(ast->to_texp()->to_string(), expected);
  } catch (const util::error::message &m) {
    m.print(std::cout, source, "source");
    throw std::runtime_error("parsing error");
  }
}

template<auto ParseFun>
void parse_equal_rethrow(std::string_view source1, std::string_view source2) {
  auto tks1 = parse::tokenizer(source1), tks2 = parse::tokenizer(source2);
  try {
    auto ast1 = ParseFun(tks1), ast2 = ParseFun(tks2);
    EXPECT_TRUE(tks1.empty());
    EXPECT_TRUE(tks2.empty());
    EXPECT_EQ(ast1->to_texp()->to_string(), ast2->to_texp()->to_string());
  } catch (const util::error::message &m) {
    m.print(std::cout, {source1, source2}, {"source1", "source2"});
    throw std::runtime_error("parsing error");
  }
}

#define MACROS_CONCAT_NAME_INNER(x, y) x##y
#define MACROS_CONCAT_NAME(x, y) MACROS_CONCAT_NAME_INNER(x, y)
#define TEST_PARSE_NS_IMPL(ns, test_name, source, expected) TEST ( \
  test_name, MACROS_CONCAT_NAME(test_name, __COUNTER__) ) {                                             \
  EXPECT_NO_THROW(parse_expect_rethrow< ast:: ns ::parse >(source,expected));                  \
}

#define TEST_PARSE_EQUAL_NS_IMPL(ns, test_name, source, expected) TEST ( \
  test_name, MACROS_CONCAT_NAME(test_name, __COUNTER__) ) {                                             \
  EXPECT_NO_THROW(parse_equal_rethrow< ast:: ns ::parse >(source,expected));                  \
}

#define TEST_PARSE_EXPRESSION(source, ...) TEST_PARSE_NS_IMPL( \
  expression, Expression, source,__VA_ARGS__)
#define TEST_PARSE_MATCHER(source, expected) TEST_PARSE_NS_IMPL( \
  matcher, Matcher, source,expected)
#define TEST_PARSE_DEFINITION(source, expected) TEST_PARSE_NS_IMPL( \
  definition , Definition, source,expected)
#define TEST_PARSE_TYPE(source, expected) TEST_PARSE_NS_IMPL( \
  type::expression, TypeExpression, source,expected)
#define TEST_PARSE_TYPE_DECL(source, expected) TEST_PARSE_NS_IMPL( \
  type::definition,TypeDef, source,expected)

#define TEST_PARSE_EXPRESSION_EQUAL(source, ...) TEST_PARSE_EQUAL_NS_IMPL( \
  expression, Expression, source,__VA_ARGS__)

using namespace util;

TEST_PARSE_EXPRESSION("Upper lower",
                      "ast::expression::constructor{name : 'Upper',  arg : ast::expression::identifier{name : 'lower'}}");
TEST_PARSE_EXPRESSION("Upper", "ast::expression::constructor{name : 'Upper',  arg : null}");
TEST(Expression, MACROS_CONCAT_NAME(Expression, __COUNTER__)) {
  EXPECT_THROW(parse_expect_rethrow<ast::expression::parse>("Upper x y", ""), std::runtime_error);
}
TEST_PARSE_EXPRESSION("fun x -> x+1",
                      "ast::expression::fun{args : [ast::matcher::universal_matcher{name : 'x'}],  body : ast::expression::fun_app{f : ast::expression::fun_app{f : ast::expression::identifier{name : '__binary_op__PLUS__'},  x : ast::expression::identifier{name : 'x'}},  x : ast::expression::literal{value : ast::literal::integer{value : 1}}}}");
TEST_PARSE_EXPRESSION("(fun x -> x+1) + y",
                      "ast::expression::fun_app{f : ast::expression::fun_app{f : ast::expression::identifier{name : '__binary_op__PLUS__'},  x : ast::expression::fun{args : [ast::matcher::universal_matcher{name : 'x'}],  body : ast::expression::fun_app{f : ast::expression::fun_app{f : ast::expression::identifier{name : '__binary_op__PLUS__'},  x : ast::expression::identifier{name : 'x'}},  x : ast::expression::literal{value : ast::literal::integer{value : 1}}}}},  x : ast::expression::identifier{name : 'y'}}"
)
TEST_PARSE_EXPRESSION("(fun x -> fn_const x) y",
                      "ast::expression::fun_app{f : ast::expression::fun{args : [ast::matcher::universal_matcher{name : 'x'}],  body : ast::expression::fun_app{f : ast::expression::identifier{name : 'fn_const'},  x : ast::expression::identifier{name : 'x'}}},  x : ast::expression::identifier{name : 'y'}}");
TEST_PARSE_EXPRESSION("fun x -> fn_const x y",
                      "ast::expression::fun{args : [ast::matcher::universal_matcher{name : 'x'}],  body : ast::expression::fun_app{f : ast::expression::fun_app{f : ast::expression::identifier{name : 'fn_const'},  x : ast::expression::identifier{name : 'x'}},  x : ast::expression::identifier{name : 'y'}}}"
);
TEST_PARSE_EXPRESSION("fun x y (a,b) None -> fn_const x y",
                      "ast::expression::fun{args : [ast::matcher::universal_matcher{name : 'x'}, ast::matcher::universal_matcher{name : 'y'}, ast::matcher::tuple_matcher{args : [ast::matcher::universal_matcher{name : 'a'}, ast::matcher::universal_matcher{name : 'b'}]}, ast::matcher::constructor_matcher{cons : 'None',  arg : null}],  body : ast::expression::fun_app{f : ast::expression::fun_app{f : ast::expression::identifier{name : 'fn_const'},  x : ast::expression::identifier{name : 'x'}},  x : ast::expression::identifier{name : 'y'}}}"
);
TEST_PARSE_EXPRESSION("fun (Constr x) -> x + 2",
                      "ast::expression::fun{args : [ast::matcher::constructor_matcher{cons : 'Constr',  arg : ast::matcher::universal_matcher{name : 'x'}}],  body : ast::expression::fun_app{f : ast::expression::fun_app{f : ast::expression::identifier{name : '__binary_op__PLUS__'},  x : ast::expression::identifier{name : 'x'}},  x : ast::expression::literal{value : ast::literal::integer{value : 2}}}}"
)
TEST_PARSE_EXPRESSION("fun Constr (x) -> x + 2",
                      "ast::expression::fun{args : [ast::matcher::constructor_matcher{cons : 'Constr',  arg : ast::matcher::universal_matcher{name : 'x'}}],  body : ast::expression::fun_app{f : ast::expression::fun_app{f : ast::expression::identifier{name : '__binary_op__PLUS__'},  x : ast::expression::identifier{name : 'x'}},  x : ast::expression::literal{value : ast::literal::integer{value : 2}}}}"
)
TEST_PARSE_EXPRESSION("fun (Constr) x -> x + 2",
                      "ast::expression::fun{args : [ast::matcher::constructor_matcher{cons : 'Constr',  arg : null}, ast::matcher::universal_matcher{name : 'x'}],  body : ast::expression::fun_app{f : ast::expression::fun_app{f : ast::expression::identifier{name : '__binary_op__PLUS__'},  x : ast::expression::identifier{name : 'x'}},  x : ast::expression::literal{value : ast::literal::integer{value : 2}}}}"
)
TEST_PARSE_EXPRESSION("lower lower",
                      "ast::expression::fun_app{f : ast::expression::identifier{name : 'lower'},  x : ast::expression::identifier{name : 'lower'}}");
TEST_PARSE_EXPRESSION("45 + 25 - 93 + twice 97 + 21",
                      "ast::expression::fun_app{f : ast::expression::fun_app{f : ast::expression::identifier{name : '__binary_op__PLUS__'},  x : ast::expression::fun_app{f : ast::expression::fun_app{f : ast::expression::identifier{name : '__binary_op__PLUS__'},  x : ast::expression::fun_app{f : ast::expression::fun_app{f : ast::expression::identifier{name : '__binary_op__MINUS__'},  x : ast::expression::fun_app{f : ast::expression::fun_app{f : ast::expression::identifier{name : '__binary_op__PLUS__'},  x : ast::expression::literal{value : ast::literal::integer{value : 45}}},  x : ast::expression::literal{value : ast::literal::integer{value : 25}}}},  x : ast::expression::literal{value : ast::literal::integer{value : 93}}}},  x : ast::expression::fun_app{f : ast::expression::identifier{name : 'twice'},  x : ast::expression::literal{value : ast::literal::integer{value : 97}}}}},  x : ast::expression::literal{value : ast::literal::integer{value : 21}}}"
);
TEST_PARSE_EXPRESSION("Error a , t ",
                      "ast::expression::build_tuple{args : [ast::expression::constructor{name : 'Error',  arg : ast::expression::identifier{name : 'a'}}, ast::expression::identifier{name : 't'}]}"
)
TEST_PARSE_MATCHER("Error _ , t ",
                   "ast::matcher::tuple_matcher{args : [ast::matcher::constructor_matcher{cons : 'Error',  arg : ast::matcher::anonymous_universal_matcher{}}, ast::matcher::universal_matcher{name : 't'}]}"
)
TEST_PARSE_EXPRESSION("match t with\n"
                      "| Some value -> Some (f value)\n"
                      "| None -> None",
                      "ast::expression::match_with{what : ast::expression::identifier{name : 't'},  branches : [ast::expression::match_with::branch{pattern : ast::matcher::constructor_matcher{cons : 'Some',  arg : ast::matcher::universal_matcher{name : 'value'}},  result : ast::expression::constructor{name : 'Some',  arg : ast::expression::fun_app{f : ast::expression::identifier{name : 'f'},  x : ast::expression::identifier{name : 'value'}}}}, ast::expression::match_with::branch{pattern : ast::matcher::constructor_matcher{cons : 'None',  arg : null},  result : ast::expression::constructor{name : 'None',  arg : null}}]}");
TEST_PARSE_MATCHER("Error _ , t ",
                   "ast::matcher::tuple_matcher{args : [ast::matcher::constructor_matcher{cons : 'Error',  arg : ast::matcher::anonymous_universal_matcher{}}, ast::matcher::universal_matcher{name : 't'}]}")
TEST_PARSE_DEFINITION("let rec f x = f x and x = f () ",
                      "ast::definition::t{rec : true,  defs : [ast::definition::def{name : ast::matcher::universal_matcher{name : 'f'},  e : ast::expression::fun{args : [ast::matcher::universal_matcher{name : 'x'}],  body : ast::expression::fun_app{f : ast::expression::identifier{name : 'f'},  x : ast::expression::identifier{name : 'x'}}}}, ast::definition::def{name : ast::matcher::universal_matcher{name : 'x'},  e : ast::expression::fun_app{f : ast::expression::identifier{name : 'f'},  x : ast::expression::literal{value : ast::literal::unit{}}}}]}"
);

TEST_PARSE_TYPE("int", "ast::type::expression::identifier{name : 'int'}");
TEST_PARSE_TYPE("int list",
                "ast::type::expression::constr{x : ast::type::expression::identifier{name : 'int'},  f : ast::type::expression::identifier{name : 'list'}}");
TEST_PARSE_TYPE("int option",
                "ast::type::expression::constr{x : ast::type::expression::identifier{name : 'int'},  f : ast::type::expression::identifier{name : 'option'}}"
);
TEST_PARSE_TYPE("int option list",
                "ast::type::expression::constr{x : ast::type::expression::constr{x : ast::type::expression::identifier{name : 'int'},  f : ast::type::expression::identifier{name : 'option'}},  f : ast::type::expression::identifier{name : 'list'}}"
);
TEST_PARSE_TYPE("int list option list",
                "ast::type::expression::constr{x : ast::type::expression::constr{x : ast::type::expression::constr{x : ast::type::expression::identifier{name : 'int'},  f : ast::type::expression::identifier{name : 'list'}},  f : ast::type::expression::identifier{name : 'option'}},  f : ast::type::expression::identifier{name : 'list'}}"
);
TEST_PARSE_TYPE("(int,bool) result",
                "ast::type::expression::constr{x : ast::type::expression::tuple{ts : [ast::type::expression::identifier{name : 'int'}, ast::type::expression::identifier{name : 'bool'}]},  f : ast::type::expression::identifier{name : 'result'}}"
);
TEST_PARSE_TYPE("(int * bool) list",
                "ast::type::expression::constr{x : ast::type::expression::product{ts : [ast::type::expression::identifier{name : 'int'}, ast::type::expression::identifier{name : 'bool'}]},  f : ast::type::expression::identifier{name : 'list'}}"
);
TEST_PARSE_TYPE("int * bool list",
                "ast::type::expression::product{ts : [ast::type::expression::identifier{name : 'int'}, ast::type::expression::constr{x : ast::type::expression::identifier{name : 'bool'},  f : ast::type::expression::identifier{name : 'list'}}]}"
);
TEST_PARSE_TYPE("int * (bool list)",
                "ast::type::expression::product{ts : [ast::type::expression::identifier{name : 'int'}, ast::type::expression::constr{x : ast::type::expression::identifier{name : 'bool'},  f : ast::type::expression::identifier{name : 'list'}}]}"
);
TEST_PARSE_TYPE("int -> int",
                "ast::type::expression::function{from : ast::type::expression::identifier{name : 'int'},  to : ast::type::expression::identifier{name : 'int'}}"
);
TEST_PARSE_TYPE("'a list -> int",
                "ast::type::expression::function{from : ast::type::expression::constr{x : ast::type::expression::identifier{name : ''a'},  f : ast::type::expression::identifier{name : 'list'}},  to : ast::type::expression::identifier{name : 'int'}}"
);
TEST_PARSE_TYPE("'a list -> ('a -> 'a -> bool) -> unit -> 'a list",
                "ast::type::expression::function{from : ast::type::expression::constr{x : ast::type::expression::identifier{name : ''a'},  f : ast::type::expression::identifier{name : 'list'}},  to : ast::type::expression::function{from : ast::type::expression::function{from : ast::type::expression::identifier{name : ''a'},  to : ast::type::expression::function{from : ast::type::expression::identifier{name : ''a'},  to : ast::type::expression::identifier{name : 'bool'}}},  to : ast::type::expression::function{from : ast::type::expression::identifier{name : 'unit'},  to : ast::type::expression::constr{x : ast::type::expression::identifier{name : ''a'},  f : ast::type::expression::identifier{name : 'list'}}}}}"
);

TEST_PARSE_TYPE_DECL("type block = int * t and t = unit -> block option",
                     "ast::type::definition::t{nonrec : false,  defs : [ast::type::definition::single_texpr{name : 'block',  type : ast::type::expression::product{ts : [ast::type::expression::identifier{name : 'int'}, ast::type::expression::identifier{name : 't'}]}}, ast::type::definition::single_texpr{name : 't',  type : ast::type::expression::function{from : ast::type::expression::identifier{name : 'unit'},  to : ast::type::expression::constr{x : ast::type::expression::identifier{name : 'block'},  f : ast::type::expression::identifier{name : 'option'}}}}]}");
TEST_PARSE_TYPE_DECL("type 'a block = 'a * 'a t and 'a t = unit -> 'a block option",
                     "ast::type::definition::t{nonrec : false,  defs : [ast::type::definition::single_texpr{name : 'block',  type : ast::type::expression::product{ts : [ast::type::expression::identifier{name : ''a'}, ast::type::expression::constr{x : ast::type::expression::identifier{name : ''a'},  f : ast::type::expression::identifier{name : 't'}}]}}, ast::type::definition::single_texpr{name : 't',  type : ast::type::expression::function{from : ast::type::expression::identifier{name : 'unit'},  to : ast::type::expression::constr{x : ast::type::expression::constr{x : ast::type::expression::identifier{name : ''a'},  f : ast::type::expression::identifier{name : 'block'}},  f : ast::type::expression::identifier{name : 'option'}}}}]}"
);
TEST_PARSE_TYPE_DECL("type 'a or_error = ('a,error) result ",
                     "ast::type::definition::t{nonrec : false,  defs : [ast::type::definition::single_texpr{name : 'or_error',  type : ast::type::expression::constr{x : ast::type::expression::tuple{ts : [ast::type::expression::identifier{name : ''a'}, ast::type::expression::identifier{name : 'error'}]},  f : ast::type::expression::identifier{name : 'result'}}}]}"
);
TEST_PARSE_TYPE_DECL("type ('a,'b) tree = ('a,'b) result ",
                     "ast::type::definition::t{nonrec : false,  defs : [ast::type::definition::single_texpr{name : 'tree',  type : ast::type::expression::constr{x : ast::type::expression::tuple{ts : [ast::type::expression::identifier{name : ''a'}, ast::type::expression::identifier{name : ''b'}]},  f : ast::type::expression::identifier{name : 'result'}}}]}"
);
TEST_PARSE_TYPE_DECL("type ('a,'b) result = | Okay of 'a | Error of 'b ",
                     "ast::type::definition::t{nonrec : false,  defs : [ast::type::definition::single_variant{name : 'result',  variants : [ast::type::definition::single_variant::constr{name : 'Okay',  type : ast::type::expression::identifier{name : ''a'}}, ast::type::definition::single_variant::constr{name : 'Error',  type : ast::type::expression::identifier{name : ''b'}}]}]}"
);

TEST_PARSE_EXPRESSION_EQUAL("4*5+27","(4*5)+27");
TEST_PARSE_EXPRESSION_EQUAL("4*5+27/34*32-27*34/32+5*(6+23)/45","(4*5)+((27/34)*32)-((27*34)/32)+((5*(6+23))/45)");
TEST_PARSE_EXPRESSION_EQUAL("- 10","__unary_op__MINUS__ 10");
TEST_PARSE_EXPRESSION_EQUAL("+ 45 + - - 23","__binary_op__PLUS__ (__unary_op__PLUS__ 45) (__unary_op__MINUS__ (__unary_op__MINUS__ 23))");
TEST_PARSE_EXPRESSION("(<)","ast::expression::identifier{name : '__binary_op__LESS_THAN__'}");
TEST_PARSE_EXPRESSION("(+)","ast::expression::identifier{name : '__binary_op__PLUS__'}");
TEST_PARSE_EXPRESSION("( * )","ast::expression::identifier{name : '__binary_op__STAR__'}");
TEST_PARSE_EXPRESSION("( *)","ast::expression::identifier{name : '__binary_op__STAR__'}");

TEST_PARSE_EXPRESSION_EQUAL("a b |> c d |> e f <| g h |> i j <| k l ","(a b |> c d |> e f) <| (g h |> i j) <| (k l) ");
TEST_PARSE_EXPRESSION_EQUAL("(a b |> c d |> e f) <| (g h |> i j) <| (k l) ","(a b |> c d |> e f) ( (g h |> i j)  (k l))");
TEST_PARSE_EXPRESSION_EQUAL("(a b |> c d |> e f) ((g h |> i j) (k l))","(e f (c d (a b))) ((i j (g h)) (k l))");

TEST_PARSE_EXPRESSION(R"( "Hello, World!" )",R"(ast::expression::literal{value : ast::literal::string{value : 'Hello, World!\0\0\0'}})");
TEST_PARSE_EXPRESSION(R"( "Hello,\n Newline!\n" )",R"(ast::expression::literal{value : ast::literal::string{value : 'Hello,\n Newline!\n\0\0\0\0\0\0\0'}})");
TEST_PARSE_EXPRESSION(R"( "Hello,\10 Newline!\10" )",R"(ast::expression::literal{value : ast::literal::string{value : 'Hello,\n Newline!\n\0\0\0\0\0\0\0'}})");
TEST_PARSE_EXPRESSION(R"( "Hello, john\"\\"  )",R"(ast::expression::literal{value : ast::literal::string{value : 'Hello, john\"\\\0\0\0'}})");
}
