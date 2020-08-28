#include <gtest/gtest.h>
#include <parse.h>


namespace {

TEST(Parse,Matcher1){
  static constexpr std::string_view source = "Error _ , t ";
  auto tks = parse::tokenizer(source);
  auto ast = ast::matcher::parse(tks);
  EXPECT_TRUE(tks.empty());
}

TEST(Parse,Expression1){
  static constexpr std::string_view source = "Upper lower";
  auto tks = parse::tokenizer(source);
  ast::expression::ptr ast;
  EXPECT_NO_THROW(ast = ast::expression::parse(tks));
  EXPECT_TRUE(tks.empty());
  EXPECT_TRUE(dynamic_cast<ast::expression::fun_app*>(ast.get()));
}

TEST(Parse,Expression2){
  static constexpr std::string_view source = "lower lower";
  auto tks = parse::tokenizer(source);
  ast::expression::ptr ast;
  EXPECT_NO_THROW(ast = ast::expression::parse(tks));
  EXPECT_TRUE(tks.empty());
  EXPECT_TRUE(dynamic_cast<ast::expression::fun_app*>(ast.get()));
}

TEST(Parse,Definition){
  static constexpr std::string_view source = "let rec f x = f x and x = f () ;;";
  auto tks = parse::tokenizer(source);
  ast::definition::ptr ast;
  EXPECT_NO_THROW(ast = ast::definition::parse(tks));
  EXPECT_NO_THROW(tks.expect_pop(parse::EOC));
  EXPECT_TRUE(tks.empty());
}

#define MACROS_CONCAT_NAME_INNER(x, y) x##y
#define MACROS_CONCAT_NAME(x, y) MACROS_CONCAT_NAME_INNER(x, y)
#define TEST_PARSE_NS_IMPL(ns,test_name,source) TEST ( \
  Parse, test_name ) {                                \
  auto tks = parse::tokenizer( source );              \
  EXPECT_NO_THROW( \
  ast:: ns ::parse(tks) );                \
  EXPECT_TRUE(\
  tks.empty());}

#define TEST_PARSE_TYPE(source) TEST_PARSE_NS_IMPL( \
  type::expression, MACROS_CONCAT_NAME(Type, __COUNTER__), source)
#define TEST_PARSE_TYPE_DECL(source) TEST_PARSE_NS_IMPL( \
  type::definition, MACROS_CONCAT_NAME(TypeDef, __COUNTER__), source)

TEST_PARSE_TYPE("int");
TEST_PARSE_TYPE("int list");
TEST_PARSE_TYPE("int option");
TEST_PARSE_TYPE("int option list");
TEST_PARSE_TYPE("int list option list");
TEST_PARSE_TYPE("(int,bool) result");
TEST_PARSE_TYPE("(int * bool) list");
TEST_PARSE_TYPE("int * bool list");
TEST_PARSE_TYPE("int * (bool list)");
TEST_PARSE_TYPE("'a list -> int");
TEST_PARSE_TYPE("'a list -> ('a -> 'a -> bool) -> unit -> 'a list");
TEST_PARSE_TYPE_DECL("type block = int * t and t = unit -> block option");
TEST_PARSE_TYPE_DECL("type 'a block = 'a * 'a t and 'a t = unit -> 'a block option");



}