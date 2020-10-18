#include <sexp.h>

namespace util::sexp {
std::string sexp_of_t::to_sexp_string() const { return to_sexp().to_string(); }


//std::optional<std::pair<std::string,std::string>> match::test_match(const t &s) const {
//  if (value.index() < 2) {
//    if (value.index() != value.index()) {
//      return std::make_pair(s.to_string(), to_string());
//    }
//    if (is_atom()) {
//      if(s.atom()==atom())return {};
//      return std::make_pair(s.to_string(), to_string());
//    } else {
//      if (s.size() != size()) {
//        return std::make_pair(s.to_string(), to_string());
//      }
//      auto itm = begin();
//      for(auto its = s.begin(); its!=s.end();++its,++itm){
//        auto o = itm->test_match(*its);
//        if(o.has_value())return std::move(o);
//      }
//      return {};
//    }
//  } else if (value.index()==2) {
//    return {};
//  } else {
//    return std::make_pair(s.to_string(), to_string());
//  }
//}
bool match::operator==(const t &s) const {
  auto s_string = s.to_string();
  auto t_string = to_string();

  if (value.index() < 2) {
    if (value.index() != s.value.index()) {
      return false;
    }
    if (is_atom()) {
      return s.atom()==atom();
    } else {
      if (s.size() != size()) {
        return false;
      }
      auto itm = begin();
      for(auto its = s.begin(); its!=s.end();++its,++itm)if(*itm!=*its)return false;
      return true;
    }
  } else if (value.index()==2) {
    return true;
  } else {
    return false;
  }
}
bool match::operator!=(const t &s) const {
  return !this->operator==(s);
}

bool operator==(const t &s, const match &m) {
  return m == s;
}
bool operator!=(const t &s, const match &m) {
  return m != s;
}
std::ostream &operator<<(std::ostream &os, const match &m) {
  return m.to_stream(os);
}
std::ostream &operator<<(std::ostream &os, const t &s) {
  return s.to_stream(os);
}

}
