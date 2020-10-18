#include <gtest/gtest.h>
#include <util/sexp.h>

namespace util::sexp{
namespace {

TEST(Sexp,Constructors) {

  t s1 = "A";
  EXPECT_TRUE(s1.is_atom());
  EXPECT_EQ(s1.to_string(),"A");
  t s2 = {"A"};
  EXPECT_TRUE(s2.is_list());
  EXPECT_EQ(s2.to_string(),"(A)");


  EXPECT_NE(s2,s1);
  EXPECT_EQ(s2[0],s1);

  t s3 = {"A","B"};
  EXPECT_TRUE(s3.is_list());
  EXPECT_EQ(s3.to_string(),"(A B)");

  t s4 = {"A","B","C"};
  EXPECT_TRUE(s4.is_list());
  EXPECT_EQ(s4.to_string(),"(A B C)");

  t s5 = {"A", {"A","B"},{"D"}, {}};
  EXPECT_TRUE(s5.is_list());
  EXPECT_EQ(s5.to_string(),"(A (A B) (D) ())");

  EXPECT_TRUE(s5[0].is_atom());
  EXPECT_TRUE(s5[1].is_list());
  EXPECT_TRUE(s5[2].is_list());
  EXPECT_TRUE(s5[3].is_list());
  EXPECT_EQ(s5[3].size(),0);
  t s6 = s5;
  EXPECT_TRUE(s6.is_list());
  EXPECT_EQ(s6.to_string(),"(A (A B) (D) ())");

  EXPECT_TRUE(s6[0].is_atom());
  EXPECT_TRUE(s6[1].is_list());
  EXPECT_TRUE(s6[2].is_list());

  EXPECT_EQ(s5,s6);
  EXPECT_NE(s5,s3);
  EXPECT_EQ(s5[0],s1);
  EXPECT_EQ(s5[1],s3);
  EXPECT_EQ(s5[1][0],s5[0]);
}

TEST(Matcher,Constructor){
  EXPECT_EQ(match(any).value.index(),2);
  EXPECT_EQ(match(none).value.index(),3);
  match m1 = {"John", any, "Appleseed"};
  t j1 = {"John", "M." , "Appleseed"};
  t j2 = {"John", "W." , "Appleseed"};
  t j3 = {"John", {"FRS","OBE"} , "Appleseed"};
  t j4 = {"John" , "Appleseed"};
  //EXPECT_SEXP_EQ( j1, {"John"} );
  /*m1.test_match(j1);
  m1.test_match(j2);
  m1.test_match(j3);
  m1.test_match(j4);*/

}


}
}