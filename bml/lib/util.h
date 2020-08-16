
#ifndef COMPILERS_BML_LIB_UTIL_H_
#define COMPILERS_BML_LIB_UTIL_H_

#include<string>
#include <stdexcept>
#include <sstream>
#include <array>

class formatter
{
 public:
  formatter() {}
  ~formatter() {}

  template <typename Type>
  formatter & operator << (const Type & value)
  {
    stream_ << value;
    return *this;
  }

  std::string str() const         { return stream_.str(); }
  operator std::string () const   { return stream_.str(); }

  enum ConvertToString
  {
    to_str
  };
  std::string operator >> (ConvertToString) { return stream_.str(); }

 private:
  std::stringstream stream_;

  formatter(const formatter &);
  formatter & operator = (formatter &);
};

template <typename... T>
constexpr auto make_array(T&&... values) ->
std::array<
    typename std::decay<
        typename std::common_type<T...>::type>::type,
    sizeof...(T)> {
  return std::array<
      typename std::decay<
          typename std::common_type<T...>::type>::type,
      sizeof...(T)>{std::forward<T>(values)...};
}

std::string_view itr_sv(std::string_view::iterator begin,std::string_view::iterator end);


#endif //COMPILERS_BML_LIB_UTIL_H_
