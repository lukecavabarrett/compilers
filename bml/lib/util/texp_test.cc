#include <gtest/gtest.h>
#include <util/texp.h>
using namespace util;
void texp_equal(const texp::ptr& t, std::string_view expected){
  EXPECT_EQ(t->to_string(),expected);
}

TEST(Texp,MakeFrom){
  texp_equal(texp::make(std::vector<int>{1,2,3,4}),"[1, 2, 3, 4]");
  texp_equal(texp::make(std::forward_list<int>{1,2,3,4}),"[1, 2, 3, 4]");
  texp_equal(texp::make(std::set<int>{1,2,3,4}),"{1, 2, 3, 4}");
  texp_equal(texp::make(std::unordered_set<int>{1,2,3,4}),"{4, 3, 2, 1}");
}

struct simple1{
  int x;
  double y;
  std::vector<int> zs;
  std::string_view name;
};

struct simple2  : public simple1, public texp::texp_of_t {
  simple2(const simple1& s) : simple1(s) {}
 public:

  texp::ptr to_texp() const final {
    return  ::util::texp::__internal::make_from_object< std::remove_cv_t< decltype(*this)> >("x,y,zs,name",x,y,zs,name);
  }
  struct __match {
    texp::ptr_init x,y,zs,name; //TODO: default should be any
    texp::ptr to_texp() {
      return  ::util::texp::__internal::make_object_from_texps< std::remove_cv_t< decltype(*this)> >("x,y,zs,name",x,y,zs,name);
    }
  };
  static_assert(std::is_aggregate_v<__match>);
  static texp::ptr match_texp(__match&& m) {
    return m.to_texp();
  }

};

struct simple3  : public simple1, public texp::texp_of_t {
  simple1 inst;
  simple3(const simple1& s) : simple1(s) {}

  TO_TEXP(x,y,zs,name,inst);

};

TEST(Texp,Obj){
  texp_equal(texp::make(simple1({.x=3,.y=3.14,.zs={1,2,3},.name="Jack"})),"simple1 = <abstr>");
  texp_equal(texp::make(simple2({.x=3,.y=3.14,.zs={1,2,3},.name="Jack"})),"simple2{name : 'Jack', y : 3.140000, zs : [1, 2, 3], x : 3}");
  texp_equal(texp::make(simple3({.x=3,.y=3.14,.zs={1,2,3},.name="Jack"})),"simple3{inst : simple1 = <abstr>, name : 'Jack', y : 3.140000, zs : [1, 2, 3], x : 3}");
  texp_equal(simple2::match_texp({.x=3}),"simple2::__match{name : <any>, y : <any>, zs : <any>, x : 3}");
}