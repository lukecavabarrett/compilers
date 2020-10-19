#ifndef COMPILERS_BML_LIB_SEXP_H_
#define COMPILERS_BML_LIB_SEXP_H_
#include <type_traits>
#include <typeinfo>
#ifndef _MSC_VER
#   include <cxxabi.h>
#endif
#include <memory>
#include <string>
#include <cstdlib>
#include <util/util.h>
#include <forward_list>
#include <unordered_set>
#include <variant>
#include <cassert>

namespace util {

template<class T>
std::string
type_name() {
  typedef typename std::remove_reference<T>::type TR;
  std::unique_ptr<char, void (*)(void *)> own
      (
#ifndef _MSC_VER
      abi::__cxa_demangle(typeid(TR).name(), nullptr,
                          nullptr, nullptr),
#else
      nullptr,
#endif
      std::free
  );
  std::string r = own != nullptr ? own.get() : typeid(TR).name();
  /*if (std::is_const<TR>::value)
    r += " const";
  if (std::is_volatile<TR>::value)
    r += " volatile";
  if (std::is_lvalue_reference<T>::value)
    r += "&";
  else if (std::is_rvalue_reference<T>::value)
    r += "&&";*/
  return r;
}

namespace sexp {
//the sexp type
struct t;
struct sexp_of_t {
  virtual t to_sexp() const = 0;
  std::string to_sexp_string() const;
};
struct t : public sexp_of_t {
  t(const t &) = default;
  std::variant<std::string, std::vector<t>> value;
  t(std::string_view s) : value(std::string(s)) {}
  t(const char *s) : value(std::string(s)) {}
  t(std::initializer_list<t> l) : value(std::vector<t>(l)) {}
  t &at(size_t index) { return std::get<1>(value).at(index); }
  auto operator[](size_t index) { return at(index); }
  auto begin() const { return std::get<1>(value).cbegin(); }
  auto end() const { return std::get<1>(value).cend(); }
  auto size() const { return std::get<1>(value).size(); }
  std::string_view atom() const { return std::get<0>(value); }
  bool is_atom() const { return value.index() == 0; }
  bool is_list() const { return value.index() == 1; }
  bool operator==(const t &o) const { return value == o.value; }
  bool operator!=(const t &o) const { return value != o.value; }
  t to_sexp() const final { return *this; }
  std::ostream &to_stream(std::ostream &os) const {
    if (value.index() == 0) {
      if (std::any_of(std::get<0>(value).begin(), std::get<0>(value).end(), ::isspace)) {
        os << "\"" << std::get<0>(value) << "\"";
      } else {
        os << std::get<0>(value);
      }

    } else {
      os << "(";
      bool pad = false;
      for (const t &x : std::get<1>(value)) {
        if (pad)os << " ";
        pad = true;
        x.to_stream(os);
      }
      os << ")";
    }
    return os;
  }
  std::ostream &to_stream_indent(std::ostream &os, size_t sh = 0) const {
    if (value.index() == 0) {
      while (sh--)os << "\t";
      os << std::get<0>(value) << std::endl;
      return os;
    } else if (std::none_of(std::get<1>(value).begin(), std::get<1>(value).end(), [](const sexp::t &s) { return s.is_list(); })) {
      while (sh--)os << "\t";
      os << "(";
      bool pad = false;
      for (const t &x : std::get<1>(value)) {
        if (pad)os << " ";
        pad = true;
        x.to_stream(os);
      }
      os << ")" << std::endl;
      return os;
    } else {
      for (size_t s = sh; s--;)os << "\t";
      os << "(\n";
      for (const t &x : std::get<1>(value)) {
        x.to_stream_indent(os, sh + 1);
      }
      while (sh--)os << "\t";
      os << ")\n";
    }
    return os;
  }
  std::string to_string() const {
    std::stringstream ss;
    to_stream(ss);
    return ss.str();
  }

};

struct match {
  struct any {
    bool operator==(const any &) const { return true; }
    bool operator!=(const any &) const { return false; }
  };
  struct none {
    bool operator==(const none &) const { return true; }
    bool operator!=(const none &) const { return false; }
  };
  match(const match &) = default;
  std::variant<std::string, std::vector<match>, any, none> value;
  match(std::string_view s) : value(std::string(s)) {}
  match(const char *s) : value(std::string(s)) {}
  match(std::initializer_list<match> l) : value(std::vector<match>(l)) {}
  match(any a) : value(a) {assert(value.index()==2);}
  match(none n) : value(n) {assert(value.index()==3);}
  match &at(size_t index) { return std::get<1>(value).at(index); }
  auto operator[](size_t index) { return at(index); }
  auto begin() const { return std::get<1>(value).cbegin(); }
  auto end() const { return std::get<1>(value).cend(); }
  auto size() const { return std::get<1>(value).size(); }
  std::string_view atom() const { return std::get<0>(value); }
  bool is_atom() const { return value.index() == 0; }
  bool is_list() const { return value.index() == 1; }
  std::ostream &to_stream(std::ostream &os) const {
    if (value.index() == 0) {
      if (std::any_of(std::get<0>(value).begin(), std::get<0>(value).end(), ::isspace)) {
        os << "\"" << std::get<0>(value) << "\"";
      } else {
        os << std::get<0>(value);
      }

    } else if (value.index() == 1) {
      os << "(";
      bool pad = false;
      for (const auto &x : std::get<1>(value)) {
        if (pad)os << " ";
        pad = true;
        x.to_stream(os);
      }
      os << ")";
    } else if (value.index() == 3) {
      os << "<*>";
    } else {
      os << "<X>";
    }
    return os;
  }

  std::string to_string() const {
    std::stringstream ss;
    to_stream(ss);
    return ss.str();
  }
  bool operator==(const match &o) const { return value == o.value; }
  bool operator!=(const match &o) const { return value != o.value; }
  bool operator==(const t &s) const;
  bool operator!=(const t &s) const;
  //std::optional<std::pair<std::string, std::string>> test_match(const t &s) const;
};

std::ostream& operator<<(std::ostream& os, const t& s);
std::ostream& operator<<(std::ostream& os, const match& m);

bool operator==(const t &s, const match &m);
bool operator!=(const t &s, const match &m);

//#define EXPECT_SEXP_EQ(sexp_e, ... ) \
//{                                     \
//auto v = static_cast<util::sexp::match>( __VA_ARGS__ ).test_match( sexp_e ));; \
//if (v.has_value()) EXPECT_EQ(v.value().first,v.value().second); \
//}

constexpr match::any any;
constexpr match::none none;

void expect_match(const sexp_of_t &s, const match &m);
void expect_match(const t &s, const match &m);

namespace __internal {

struct try_get_as_string {
  static t get_sexp(std::string_view s) {
    return s;
  }
};

template<typename T>
struct try_convert_to_string {
  static t get_sexp(const T &v) {
    return t(std::to_string(v));
  }
};

struct try_using_sexpable_base_class {
  static t get_sexp(const sexp_of_t &s) {
    return s.to_sexp();
  }
};

template<typename T>
struct sexp_of_single {
  static t get_sexp(const T &p) {
    typedef std::conditional_t<std::is_base_of_v<sexp_of_t, T>, try_using_sexpable_base_class,
                               std::conditional_t<std::is_fundamental_v<T>, try_convert_to_string<T>, try_get_as_string>

    > strategy;
    return strategy::get_sexp(p);
  }
};

template<typename C>
struct sexp_of_single<std::unique_ptr<C> > {
  static t get_sexp(const std::unique_ptr<C> &p) {
    return sexp_of_single<C *>::get_sexp(p.get());
  }
};

template<typename C>
struct sexp_of_single<std::shared_ptr<C> > {
  static t get_sexp(const std::shared_ptr<C> &p) {
    return sexp_of_single<C *>::get_sexp(p.get());
  }
};

template<typename C>
struct sexp_of_single<C *> {
  static t get_sexp(const C *p) {
    return p == nullptr ? t("NULL") : sexp_of_single<C>::get_sexp(*p);
  }
};

template<typename C>
struct sexp_of_single<std::vector<C>> {
  static t get_sexp(const std::vector<C> &v) {
    t s = {};
    for (const C &x : v)std::get<1>(s.value).push_back(sexp_of_single<C>::get_sexp(x));
    return s;
  }
};

template<typename C>
struct sexp_of_single<std::forward_list<C>> {
  static t get_sexp(const std::forward_list<C> &v) {
    t s = {};
    for (const C &x : v)std::get<1>(s.value).push_back(sexp_of_single<C>::get_sexp(x));
    return s;
  }
};

template<typename K, typename V>
struct sexp_of_single<std::unordered_map<K, V>> {
  static t get_sexp(const std::unordered_map<K, V> &m) {
    t s = {};
    for (const auto&[k, v] : m)std::get<1>(s.value).push_back({sexp_of_single<K>::get_sexp(k), sexp_of_single<V>::get_sexp(v)});
    return s;
  }
};

template<typename T>
struct sexp_of_single<std::unordered_set<T>> {
  static t get_sexp(const std::unordered_set<T> &m) {
    t s = {};
    for (const T &x : m)std::get<1>(s.value).push_back(sexp_of_single<T>::get_sexp(x));
    return s;
  }
};

template<typename ...Ts>
t make_from(const Ts &... fields) {
  return {sexp_of_single<Ts>::get_sexp(fields) ...};
}

template<typename T>
constexpr auto type_name_old() noexcept {
  std::string_view name, prefix, suffix;
#ifdef __clang__
  name = __PRETTY_FUNCTION__;
  prefix = "auto type_name() [T = ";
  suffix = "]";
#elif defined(__GNUC__)
  name = __PRETTY_FUNCTION__;
  prefix = "constexpr auto type_name() [with T = ";
  suffix = "]";
#elif defined(_MSC_VER)
  name = __FUNCSIG__;
  prefix = "auto __cdecl type_name<";
  suffix = ">(void) noexcept";
#endif
  name.remove_prefix(prefix.size());
  name.remove_suffix(suffix.size());
  return name;
}



template<typename T>
t prepend_type(t s) {
  typedef std::remove_cv_t<std::remove_reference_t<T>> Tclean;
  static const std::string name = type_name<Tclean>();
  std::get<1>(s.value).insert(std::get<1>(s.value).begin(), t(name));
  return s;
}

}

template<typename T>
t make_sexp(const T &x) {
  return __internal::sexp_of_single<T>::get_sexp(x);
}

#define TO_SEXP(...) sexp::t to_sexp() const final {\
  return  ::util::sexp::__internal::prepend_type<decltype(*this)>( ::util::sexp::__internal::make_from(  __VA_ARGS__  ) ); \
}

}

}
#endif //COMPILERS_BML_LIB_SEXP_H_
