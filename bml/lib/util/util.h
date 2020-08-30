#ifndef COMPILERS_BML_LIB_UTIL_H_
#define COMPILERS_BML_LIB_UTIL_H_

#include<string>
#include <stdexcept>
#include <sstream>
#include <array>
#include <algorithm>
#include <fstream>
#include <streambuf>
namespace util {

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

#endif //COMPILERS_BML_LIB_UTIL_H_