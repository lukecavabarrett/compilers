
#include <message.h>

#include <algorithm>
#include <ostream>

namespace util::error {

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

void message::print(std::ostream &os,
                    std::initializer_list<std::string_view> files,
                    std::initializer_list<std::string_view> filenames) const {
  std::string_view tk = get_code_token();
  size_t i = 0;
  for (std::string_view file : files) {
    if (!file.empty() && tk.begin() >= file.begin() && tk.end() <= file.end()) {

      if (i < filenames.size()) {
        auto name = filenames.begin();
        std::advance(name, i);
        return print(os, file, *name);
      } else {
        return print(os, file,"unknown_source");
      }
    }
    ++i;
  }
  return print(os, "","unknown_source");

}

void message::print(std::ostream &os, std::string_view file, std::string_view filename) const {

  constexpr std::string_view bold_style = "\e[1m";
  constexpr std::string_view clear_style = "\e[0m";
  constexpr std::string_view sep = ":";
  constexpr std::string_view ssep = ": ";
  std::string_view err_type = get_msgtype();
  std::string_view err_style = get_msgstyle();

  std::string_view tk = get_code_token();
  std::size_t posy, posx;
  os << bold_style;
  if (!file.empty() && tk.begin() >= file.begin() && tk.end() <= file.end()) {
    posy = std::count(file.begin(), tk.begin(), 10) + 1;
    posx = std::distance(std::find(std::string_view::const_reverse_iterator(tk.begin() + 1), file.crend(), 10).base(),
                         tk.begin()) + 1;
    os << filename << sep << posy << sep << posx << ssep;
  }
  os << err_style << err_type << ssep << clear_style;
  print_content(os);
  os << std::endl;
  if (!file.empty() && tk.begin() >= file.begin() && tk.end() <= file.end()) {

    auto it_endline = tk.begin();
    if (auto ite = std::find(tk.begin(), tk.end(), 10);ite != tk.end()) {
      it_endline = ite;
      tk.remove_suffix(std::distance(ite, tk.end()));
    } else {
      it_endline = std::find(tk.end(), file.end(), 10);
    }
    for (int w = 5 - std::to_string(posy).size(); w--;)os << ' ';
    os << posy << " | " << make_string_view(tk.begin() - posx + 1, tk.begin()) << bold_style << err_style << tk
       << clear_style << make_string_view(tk.end(), it_endline) << std::endl;
    os << "      |" << std::string(posx, ' ') << bold_style << err_style << "^";
    for (int w = tk.size() - 1; w--;)os << '~';
    os << clear_style << std::endl;
  }
}
std::string_view message::get_msgstyle() const { return ""; }
namespace style {
std::string_view error::get_msgtype() const { return "error"; }
std::string_view error::get_msgstyle() const { return "\e[31m"; }
std::string_view note::get_msgtype() const { return "note"; }
std::string_view note::get_msgstyle() const { return "\e[36m"; }
std::string_view warning::get_msgtype() const { return "warning"; }
std::string_view warning::get_msgstyle() const { return "\e[35m"; }
}



//std::ostream &operator<<(std::ostream &os, const message &em) {
//  em.print(os);
//  return os;
//}
}
