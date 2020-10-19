#ifndef COMPILERS_BML_LIB_UTIL_TEXP_H_
#define COMPILERS_BML_LIB_UTIL_TEXP_H_

#include <vector>
#include <memory>
#include <unordered_map>
#include <iostream>
#include <forward_list>
#include <unordered_set>
#include <set>
#include <util/sexp.h>
namespace util{

namespace texp {

struct t {
  virtual void to_stream(std::ostream&) const = 0 ;
  std::string to_string() const {
    std::stringstream ss;
    to_stream(ss);
    return ss.str();
  }
};


typedef std::unique_ptr<t> ptr;

struct texp_of_t {
  virtual ptr to_texp() const = 0;
};


struct list : public t {
  std::vector<ptr> elems;
  list() = default;
  list(list&&) = default;
  list(std::vector<ptr>&& e) : elems(std::move(e)) {}
  void to_stream(std::ostream& os) const final {
    os << "[";
    bool comma = false;
    for(const auto &p : elems){
      if(comma)os << ", ";
      comma = true;
      p->to_stream(os);
    }
    os <<"]";
  }
};

struct object : public t {
  std::string name;
  std::unordered_map<std::string_view,ptr> fields;
  void to_stream(std::ostream& os) const final {
    os << name << "{";
    bool comma = false;
    for(const auto &[f,v] : fields){
      if(comma)os << ", ";
      comma = true;
      os << f << " : ";
      v->to_stream(os);
    }
    os <<"}";
  }
};

struct dict : public t {
  std::vector<std::pair<ptr,ptr>> elems;
  void to_stream(std::ostream& os) const final {
    os  << "{";
    bool comma = false;
    for(const auto &[f,v] : elems){
      if(comma)os << ", ";
      comma = true;
      f->to_stream(os);
      os << " : ";
      v->to_stream(os);
    }
    os <<"}";
  }
};

struct set : public t {
  std::vector<ptr> elems;
  void to_stream(std::ostream& os) const final {
    os  << "{";
    bool comma = false;
    for(const auto &v : elems){
      if(comma)os << ", ";
      comma=true;
      v->to_stream(os);
    }
    os <<"}";
  }
};

struct string : public t {
  std::string s;
  string(std::string&& s) : s(std::move(s)){}
  void to_stream(std::ostream& os) const final { os << '\''<<s<<'\''; }
};

struct null : public t {
  void to_stream(std::ostream& os) const final { os << "null"; }
};

template<typename T>
struct stringible : public t {
  T value;
  stringible(const T& v) : value(v) {}
  void to_stream(std::ostream& os) const final { os << std::to_string(value);/*<<":"<< type_name<T>();*/ }
};

template<typename T>
struct abstr : public t {
  abstr() {}
  void to_stream(std::ostream& os) const final { os << type_name<T>() << " = <abstr>"; }
};

struct any : public t {
  any() {}
  void to_stream(std::ostream& os) const final { os << "<any>"; }
};

namespace __internal {

struct try_get_as_string {
  static ptr get_texp(std::string_view s) {
    return std::make_unique<string>(std::string(s));
  }
};

template<typename T>
struct try_convert_to_string {
  static ptr get_texp(const T &v) {
    return std::make_unique<stringible<T>>(v);
  }
};

template<typename T>
struct make_abstr {
  static ptr get_texp(const T &v) {
    return std::make_unique<abstr<T>>();
  }
};

struct try_using_texpable_base_class {
  static ptr get_texp(const texp_of_t &s) {
    return s.to_texp();
  }
};

template<typename T>
struct texp_of_single {
  static ptr get_texp(const T &p) {
    typedef std::conditional_t<std::is_base_of_v<texp_of_t, T>, try_using_texpable_base_class,
                               std::conditional_t<std::is_fundamental_v<T>, try_convert_to_string<T>, make_abstr<T>>

    > strategy;
    return strategy::get_texp(p);
  }
};

template<>
struct texp_of_single<std::string> {
  static ptr get_texp(const std::string &p) {return try_get_as_string::get_texp(p);}
};template<>
struct texp_of_single<std::string_view> {
  static ptr get_texp(const std::string_view &p) {return try_get_as_string::get_texp(p);}
};


template<typename C>
struct texp_of_single<std::unique_ptr<C> > {
  static ptr get_texp(const std::unique_ptr<C> &p) {
    return texp_of_single<C *>::get_texp(p.get());
  }
};
template<typename C>
struct texp_of_single<std::shared_ptr<C> > {
  static ptr get_texp(const std::shared_ptr<C> &p) {
    return texp_of_single<C *>::get_texp(p.get());
  }
};

template<typename C>
struct texp_of_single<C *> {
  static ptr get_texp(const C *p) {
    return p == nullptr ? std::make_unique<null> : texp_of_single<C>::get_texp(*p);
  }
};

template<typename C>
struct texp_of_single<std::vector<C>> {
  static ptr get_texp(const std::vector<C> &v) {
    auto s = std::make_unique<list>();
    for (const C &x : v)s->elems.push_back(texp_of_single<C>::get_texp(x));
    return std::move(s);
  }
};

template<typename C>
struct texp_of_single<std::forward_list<C>> {
  static ptr get_texp(const std::forward_list<C> &v) {
    auto s = std::make_unique<list>();
    for (const C &x : v)s->elems.push_back(texp_of_single<C>::get_texp(x));
    return std::move(s);
  }
};

template<typename K, typename V>
struct texp_of_single<std::unordered_map<K, V>> {
  static ptr get_texp(const std::unordered_map<K, V> &m) {
    auto s = std::make_unique<dict>();
    for (const auto&[k, v] : m)s->elems.push_back({texp_of_single<K>::get_texp(k), texp_of_single<V>::get_texp(v)});
    return std::move(s);
  }
};

template<typename T>
struct texp_of_single<std::unordered_set<T>> {
  static ptr get_texp(const std::unordered_set<T> &m) {
    auto s = std::make_unique<set>();
    for (const auto&k : m)s->elems.push_back(texp_of_single<T>::get_texp(k));
    return std::move(s);
  }
};

template<typename T>
struct texp_of_single<std::set<T>> {
  static ptr get_texp(const std::set<T> &m) {
    auto s = std::make_unique<set>();
    for (const auto&k : m)s->elems.push_back(texp_of_single<T>::get_texp(k));
    return std::move(s);
  }
};



template<typename T,typename ...Ts>
ptr make_from_object(std::string_view comma_separated_names,const Ts &... fields) {
  assert(std::count(comma_separated_names.begin(),comma_separated_names.end(),',')==(sizeof...(fields))-1); //TODO: make this a static assert
  auto ita = comma_separated_names.begin();

  auto o = std::make_unique<object>();
  o->name = type_name<T>();
  for(t* p : {texp_of_single<Ts>::get_texp(fields).release() ...}){
    auto itb = std::find(ita,comma_separated_names.end(),',');
    o->fields.try_emplace(itr_sv(ita,itb),p);
    ita = itb+1;
  }
  return std::move(o);
}

template<typename T,typename ...Ts>
ptr make_object_from_texps(std::string_view comma_separated_names,Ts &... fields) {
  assert(std::count(comma_separated_names.begin(),comma_separated_names.end(),',')==(sizeof...(fields))-1); //TODO: make this a static assert
  auto ita = comma_separated_names.begin();

  auto o = std::make_unique<object>();
  o->name = type_name<T>();
  for(t* p : {fields.p.release() ...}){
    auto itb = std::find(ita,comma_separated_names.end(),',');
    if(p== nullptr)p = new any();
    o->fields.try_emplace(itr_sv(ita,itb),p);
    ita = itb+1;
  }
  return std::move(o);
}
/*
template<typename T>
ptr make_object_from_texps(std::string_view comma_separated_names) {
  assert(comma_separated_names.empty());

  auto o = std::make_unique<object>();
  o->name = type_name<T>();
  return std::move(o);
}

template<typename T,typename ...Ts>
ptr make_object_from_texps(std::string_view comma_separated_names, ptr& f, Ts&... fields) {
  assert(std::count(comma_separated_names.begin(),comma_separated_names.end(),',')==(sizeof...(fields))-1); //TODO: make this a static assert
  auto ita = comma_separated_names.begin();
  auto itb = std::find(ita,comma_separated_names.end(),',');
  std::string_view field_name = itr_sv(ita,itb);
  if(itb!=comma_separated_names.end())++itb;
  comma_separated_names = itr_sv(itb,comma_separated_names.end());

  auto o = make_object_from_texps<T>(comma_separated_names,std::forward<Ts&...>(fields)...);
  o->fields.try_emplace(field_name,f.release());
  return std::move(o);
}*/


}

template<typename T>
ptr make(const T &x) {
  return __internal::texp_of_single<T>::get_texp(x);
}
//TODO: matchers

struct ptr_init {
  ptr p;
  ptr_init() : p(std::make_unique<any>()) {}
  ptr_init(ptr_init&&) = default;
  ptr_init(ptr&& p) : p(std::move(p)) {}

  template<typename T>
  ptr_init(const T &x) : p (__internal::texp_of_single<T>::get_texp(x)) {}



};

}

}

#define TO_TEXP(...) \
 public:\
util::texp::ptr to_texp() const final {\
return  ::util::texp::__internal::make_from_object< std::remove_cv_t< decltype(*this)> >( #__VA_ARGS__ , __VA_ARGS__ );\
}

#endif //COMPILERS_BML_LIB_UTIL_TEXP_H_
