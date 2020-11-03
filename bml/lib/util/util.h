#ifndef COMPILERS_BML_LIB_UTIL_H_
#define COMPILERS_BML_LIB_UTIL_H_

#include<string>
#include <stdexcept>
#include <sstream>
#include <array>
#include <algorithm>
#include <fstream>
#include <streambuf>
namespace ast::expression{
struct fun;
}
namespace util {


struct sections_t {
  std::ostream &data, &text, &main;
  ast::expression::fun* def_fun;
  sections_t(const sections_t&) = default;
  sections_t(std::ostream& d,std::ostream& t, std::ostream& m, ast::expression::fun* df = nullptr) : data(d), text(t), main(m), def_fun(df) {}
  sections_t with_main(std::ostream& os,ast::expression::fun* df)  {
    return sections_t(data,text,os, df);
  }
  sections_t with_main(std::ostream& os)  {
    return sections_t(data,text,os, def_fun);
  }
};


std::string load_file(std::string_view path);

class formatter {
 public:
  formatter() {}
  ~formatter() {}

  template<typename Type>
  formatter &operator<<(const Type &value) {
    stream_ << value;
    return *this;
  }

  std::string str() const { return stream_.str(); }
  operator std::string() const { return stream_.str(); }

  enum ConvertToString {
    to_str
  };
  std::string operator>>(ConvertToString) { return stream_.str(); }

 private:
  std::stringstream stream_;

  formatter(const formatter &);
  formatter &operator=(formatter &);
};

template<typename... T>
constexpr auto make_array(T &&... values) ->
std::array<
    typename std::decay<
        typename std::common_type<T...>::type>::type,
    sizeof...(T)> {
  return std::array<
      typename std::decay<
          typename std::common_type<T...>::type>::type,
      sizeof...(T)>{std::forward<T>(values)...};
}

std::string_view itr_sv(std::string_view::iterator begin, std::string_view::iterator end);

template<typename T>
bool is_in(const T &v, std::initializer_list<T> lst) {
  return std::find(std::begin(lst), std::end(lst), v) != std::end(lst);
}

}

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define AT __FILE__ ":" TOSTRING(__LINE__)
#define THROW_UNIMPLEMENTED throw std::runtime_error( AT ": unimplemented" );
#define THROW_WORK_IN_PROGRESS throw std::runtime_error( AT ": work in progress" );

#endif //COMPILERS_BML_LIB_UTIL_H_
