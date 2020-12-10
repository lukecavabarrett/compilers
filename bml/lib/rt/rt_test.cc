#include <gtest/gtest.h>
#include "rt2.h"

namespace {



TEST(Value,IntBackAndForth) {
  auto ibaf = [](int64_t x) -> int64_t {
    return rt::value::from_int(x).to_int();
  };
  for(int i = -10;i<=10;++i)EXPECT_EQ(i,ibaf(i)); // [-10,10]

  EXPECT_NE(ibaf(INT64_MAX),INT64_MAX);
  EXPECT_NE(ibaf(INT64_MIN),INT64_MIN);

  EXPECT_EQ(ibaf(rt::INT63_MAX),rt::INT63_MAX);
  EXPECT_EQ(ibaf(rt::INT63_MIN),rt::INT63_MIN);

  EXPECT_NE(ibaf(rt::INT63_MAX+1),rt::INT63_MAX+1);
  EXPECT_NE(ibaf(rt::INT63_MIN-1),rt::INT63_MIN-1);

  EXPECT_EQ(rt::value::from_int(-1).v,-1);
  EXPECT_EQ(rt::value::from_int(-2).v,-3);

}



TEST(Value,UintBackAndForth) {
  auto ubaf = [](uint64_t x) ->uint64_t {
    return rt::value::from_uint(x).to_uint();
  };
  for(int i = 0;i<=20;++i)EXPECT_EQ(i,ubaf(i)); // [0,20]

  EXPECT_NE(ubaf(UINT64_MAX),UINT64_MAX);

  EXPECT_EQ(ubaf(rt::UINT63_MAX),rt::UINT63_MAX);
  EXPECT_EQ(ubaf(rt::INT63_MAX+1),rt::INT63_MAX+1);


  EXPECT_NE(ubaf(rt::UINT63_MAX+1),rt::UINT63_MAX+1);

}



TEST(Function,Sum){

  rt::value f = rt::value::from_block(&rt::int_sum);
  EXPECT_TRUE(f.is_block());
  rt::value f_3 = rt::apply_fn(f,rt::value::from_int(3));
  EXPECT_TRUE(f_3.is_block());
  rt::value f_3_5 = rt::apply_fn(f_3,rt::value::from_int(5));
  EXPECT_TRUE(f_3_5.is_immediate());
  EXPECT_EQ(f_3_5.to_int(),8);

  rt::value f_3_4 = rt::apply_fn(f_3,rt::value::from_int(4));
  EXPECT_TRUE(f_3_4.is_immediate());
  EXPECT_EQ(f_3_4.to_int(),7);

}

}
