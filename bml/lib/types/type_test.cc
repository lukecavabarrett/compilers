#include <gtest/gtest.h>
#include <types/types.h>

using namespace type::function;
using namespace type::expression::placeholder;

TEST(Type, Print1) {
  std::stringstream s;
  auto t = tf_fun(tf_int, tf_fun(tf_int, tf_int));
  (s << t);
  EXPECT_EQ(s.str(), "int -> int -> int");
}

TEST(Type, Print1b) {
  std::stringstream s;
  auto t = tf_fun(tf_fun(tf_int, tf_int), tf_int);

  (s << t);
  EXPECT_EQ(s.str(), "(int -> int) -> int");
}

TEST(Type, Print1c) {
  std::stringstream s;
  auto t = tf_fun(tf_tuple(2)(tf_int, tf_int), tf_int);
  (s << t);
  EXPECT_EQ(s.str(), "int * int -> int");
}

TEST(Type, Print2) {
  std::stringstream s;
  auto t = tf_tuple(3)(tf_int, tf_fun(tf_int, tf_int), tf_tuple(2)(_a,_b));
  (s << t);
  EXPECT_EQ(s.str(), "int * (int -> int) * ('a * 'b)");
}