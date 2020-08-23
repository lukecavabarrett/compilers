
#ifndef COMPILERS_BML_LIB_UTIL_H_
#define COMPILERS_BML_LIB_UTIL_H_

#include<string>
#include <stdexcept>
#include <sstream>
#include <array>

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

// Type your code here, or load an example.
#include <variant>
#include <vector>
#include <cassert>

template <typename T,int I>
class struct_wrap : public T {
  using T::T;
};

template <typename T,int I>
class prim_wrap  {
  T value_;
 public:
  prim_wrap( T&& x ) noexcept :value_(x)  {}
  prim_wrap(const T& x ) noexcept :value_(x)  {}
  constexpr operator T& () noexcept {return value_;}
};

template<typename T,int I = 0>
using wrap = std::conditional_t<std::is_class_v<T>,struct_wrap<T,I>,prim_wrap<T,I>>;

template<typename T,typename E>
class result {
  template<typename To,typename Eo> friend class result;

  struct error_tag {};
 public:

  constexpr result(const result& r) = default;
  constexpr result(result&& r) = default;

  constexpr result(const wrap<E,1>& e) : value_(std::in_place_index_t<1>(),e) {}
  constexpr result(wrap<E,1>&& e) : value_(std::in_place_index_t<1>(),std::move(e)) {}

  template<typename U>
  constexpr result(const U& t) : value_(std::in_place_index_t<0>(),t) {}

  template<typename U>
  constexpr result(U&& t) : value_(std::in_place_index_t<0>(),std::move(t)) {}

  template<class... Args >
  constexpr result(Args&&... args ) : value_(std::in_place_index_t<0>(),std::forward<Args&&>(args)...) {}

  constexpr explicit result(error_tag,const E& e) : value_(std::in_place_index_t<1>(),e) {}
  template<class... Args >
  constexpr explicit result(error_tag,Args&&... args ) : value_(std::in_place_index_t<1>(),std::forward<Args&&>(args)...) {}

  constexpr static result error(const E& e) {return result(error_tag(),e);}
  template<class... Args >
  constexpr static result error(Args&&... args ) {return result(error_tag(),std::forward<Args&&>(args)...);}

  constexpr bool ok() const noexcept {return value_.index()==0;}
  constexpr T& value() {
    return std::get<0>(value_);
  }
  constexpr T& value() const {
    return std::get<0>(value_);
  }

  constexpr E& error() {
    return std::get<1>(value_);
  }
  constexpr E& error() const {
    return std::get<1>(value_);
  }

  typedef wrap<E,1> result_error ;
  std::variant<T,E> value_;

 private:

};


#define RETURN_IF_ERROR(expr)                                                \
  do {                                                                       \
    /* Using _status below to avoid capture problems if expr is "status". */ \
    const  _status = (expr);              \
    if (PROTOBUF_PREDICT_FALSE(!_status.ok())) return _status;               \
  } while (0)

#define RETURN_IF_ERROR_BIND(expr)                                                \
  do {                                                                       \
    /* Using _status below to avoid capture problems if expr is "status". */ \
    const PROTOBUF_NAMESPACE_ID::util::Status _status = (expr);              \
    if (PROTOBUF_PREDICT_FALSE(!_status.ok())) return _status;               \
  } while (0)

// Internal helper for concatenating macro values.
#define STATUS_MACROS_CONCAT_NAME_INNER(x, y) x##y
#define STATUS_MACROS_CONCAT_NAME(x, y) STATUS_MACROS_CONCAT_NAME_INNER(x, y)

#define ASSIGN_OR_RETURN_IMPL(status, lhs, rexpr) \
  auto status = (rexpr); \
  if (!status.ok()) return decltype(rexpr)::result_error(std::move(status.error())); \
  lhs = std::move(status.value())

#define ASSIGN_OR_RETURN(lhs, rexpr) \
  ASSIGN_OR_RETURN_IMPL( \
      STATUS_MACROS_CONCAT_NAME(_status_or_value, __COUNTER__), lhs, rexpr);

#define ASSIGN_OR_RETURN_BIND_IMPL(status, lhs, rexpr) \
  Status status = (rexpr); \
  if (!status.ok()) return status.error(); \
  lhs = std::move(status.value())

#define ASSIGN_OR_RETURN_BIND(lhs, rexpr) \
  ASSIGN_OR_RETURN_IMPL( \
      STATUS_MACROS_CONCAT_NAME(_status_or_value, __COUNTER__), lhs, rexpr);

#endif //COMPILERS_BML_LIB_UTIL_H_
