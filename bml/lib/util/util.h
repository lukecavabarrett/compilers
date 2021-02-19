#ifndef COMPILERS_BML_LIB_UTIL_H_
#define COMPILERS_BML_LIB_UTIL_H_

#include<string>
#include <stdexcept>
#include <sstream>
#include <array>
#include <algorithm>
#include <fstream>
#include <streambuf>
namespace ast::expression {
struct fun;
}
namespace util {

std::string load_file(std::string_view path);

template<typename V>
// recursion-ender
void multi_emplace(std::vector<V> &vec) {}

template<typename V, typename T1, typename... Types>
void multi_emplace(std::vector<V> &vec, T1 &&t1, Types &&... args) {
  vec.emplace_back(std::move(t1));
  multi_emplace(vec, args...);
}

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

template<class... Ts>
struct overloaded : Ts ... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;
namespace chars {

bool is_escaped_in_string_literal(char c);
bool is_escaped_in_asm_string_literal(char c);
bool has_escaped_mnemonic(char c);
char escaped_mnemonic(char c);
bool is_valid_mnemonic(char c);
char parse_mnemonic(char c);
}
}

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define AT __FILE__ ":" TOSTRING(__LINE__)
#define THROW_UNIMPLEMENTED throw std::runtime_error( AT ": unimplemented" );
#define THROW_WORK_IN_PROGRESS throw std::runtime_error( AT ": work in progress" );
#define THROW_INTERNAL_ERROR throw std::runtime_error( AT ": internal_error" );

#endif //COMPILERS_BML_LIB_UTIL_H_
