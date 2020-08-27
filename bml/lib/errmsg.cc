
#include <errmsg.h>

#include <algorithm>
#include <ostream>

namespace error {

namespace {
template<typename _It>
inline constexpr auto make_string_view(_It begin, _It end) {
  using result_type = std::basic_string_view<typename std::iterator_traits<_It>::value_type>;

  return result_type{
      (begin != end) ? &*begin : nullptr, (typename result_type::size_type)
          std::max(std::distance(begin, end), (typename result_type::difference_type) 0)
  };
}
}

void errmsg::print(std::ostream &os,std::string_view file,std::string_view filename) const {

  constexpr std::string_view bold_style = "\e[1m";
  constexpr std::string_view clear_style = "\e[0m";
  constexpr std::string_view sep = ":";
  constexpr std::string_view ssep = ": ";
  std::string_view err_type = get_msgtype();
  std::string_view err_style = get_msgstyle();

  std::string_view tk = get_code_token();
  std::size_t posy, posx;
  os << bold_style;
  if (!file.empty()) {
    posy = std::count(file.begin(), tk.begin(), 10) + 1;
    posx = std::distance(std::find(std::string_view::const_reverse_iterator(tk.begin() + 1), file.crend(), 10).base(), tk.begin()) + 1;
    os << filename << sep << posy << sep << posx << ssep;
  }
  os << err_style << err_type << ssep << clear_style;
  print_content(os);
  os << std::endl;
  if (!file.empty()) {

    auto it_endline = tk.begin();
    if (auto ite = std::find(tk.begin(), tk.end(), 10);ite != tk.end()) {
      it_endline = ite;
      tk.remove_suffix(std::distance(ite, tk.end()));
    } else {
      it_endline = std::find(tk.end(), file.end(), 10);
    }
    for (int w = 5 - std::to_string(posy).size(); w--;)os << ' ';
    os << posy << " | " << make_string_view(tk.begin() - posx + 1, tk.begin()) << bold_style << err_style << tk << clear_style << make_string_view(tk.end(), it_endline) << std::endl;
    os << "      |" << std::string(posx, ' ') << bold_style << err_style << "^";
    for (int w = tk.size() - 1; w--;)os << '~';
    os << clear_style << std::endl;
  }
}
std::string_view errmsg::get_msgstyle() const { return ""; }

void report_token_error::print_content(std::ostream &os) const {
  constexpr std::string_view bold_style = "\e[1m";
  constexpr std::string_view clear_style = "\e[0m";
  os << msg_front << " '" << bold_style << token << clear_style << "' " << msg_back;
}
std::string_view report_token_error::get_code_token() const { return token; }
report_token_error::report_token_error(std::string_view f, std::string_view t, std::string_view b) : msg_front(f), token(t), msg_back(b) {}
std::string_view error::get_msgtype() const { return "error"; }
std::string_view error::get_msgstyle() const { return "\e[31m"; }
std::string_view note::get_msgtype() const { return "note"; }
std::string_view note::get_msgstyle() const { return "\e[36m"; }

std::string_view generic_static_error::get_code_token() const { return token; }
void generic_static_error::print_content(std::ostream &os) const { os << msg; }
//std::ostream &operator<<(std::ostream &os, const errmsg &em) {
//  em.print(os);
//  return os;
//}
std::string_view warning::get_msgtype() const { return "warning"; }
std::string_view warning::get_msgstyle() const { return "\e[35m"; }
}
