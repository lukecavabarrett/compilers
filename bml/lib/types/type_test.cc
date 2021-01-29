#include <gtest/gtest.h>
#include <types/types.h>

TEST(Type, Print) {
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
  EXPECT_EQ(s.str(), "(int -> (int -> int))");
  //TODO: have this to be printed as "int -> int -> int"
}