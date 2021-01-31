#include <gtest/gtest.h>
#include <types/types.h>

TEST(Type, Print1) {
  std::stringstream s;
  type::expression::t t(&type::function::tf_fun, {
      &type::function::tf_int,
      type::expression::t{
          &type::function::tf_fun, {
              &type::function::tf_int,
              &type::function::tf_int
          }}
  });
  (s << t);
  EXPECT_EQ(s.str(), "int -> int -> int");
}

TEST(Type, Print1b) {
  std::stringstream s;
  type::expression::t t(&type::function::tf_fun, {
      type::expression::t{
          &type::function::tf_fun, {
              &type::function::tf_int,
              &type::function::tf_int
          }},
      &type::function::tf_int
  });
  (s << t);
  EXPECT_EQ(s.str(), "(int -> int) -> int");
}

TEST(Type, Print1c) {
  std::stringstream s;
  type::expression::t t(&type::function::tf_fun, {
      type::expression::t{
          &type::function::tf_tuple(2), {
              &type::function::tf_int,
              &type::function::tf_int
          }},
      &type::function::tf_int
  });
  (s << t);
  EXPECT_EQ(s.str(), "int * int -> int");
}

TEST(Type, Print2) {
  std::stringstream s;
  type::expression::t t(&type::function::tf_tuple(3), {
      &type::function::tf_int,
      type::expression::t{
          &type::function::tf_fun, {
              &type::function::tf_int,
              &type::function::tf_int
          }},
      type::expression::t{
          &type::function::tf_tuple(2), {
              type::expression::variable(0),
              type::expression::variable(1)
          }}
  });
  (s << t);
  EXPECT_EQ(s.str(), "int * (int -> int) * ('a * 'b)");
}