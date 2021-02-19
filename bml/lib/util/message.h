#ifndef COMPILERS_BML_LIB_ERRMSG_H_
#define COMPILERS_BML_LIB_ERRMSG_H_
#include <string>
#include <iostream>
#include <vector>
#include <memory>
#include <util/util.h>
namespace util::message {

struct base {
  virtual void print(std::ostream &) const = 0;
  virtual void link_file(std::string_view file, std::string_view filename = "source.ml") = 0;
  virtual ~base() = default;
};

struct vector : public virtual base, public std::vector<std::unique_ptr<base>> {
  typedef std::vector<std::unique_ptr<base>> vec_t;
  using vec_t::vec_t;
  void print(std::ostream &os) const final {
    for (const auto &p : static_cast<const vec_t &>(*this))p->print(os);
  }
  void link_file(std::string_view file, std::string_view filename) final {
    for (auto &p : static_cast<const vec_t &>(*this))p->link_file(file, filename);
  }

};

struct string : public virtual base {
  std::string_view what;
  string(std::string_view what) : what(what) {}
  void print(std::ostream &os) const final {
    os << what << std::endl;
  }
  void link_file(std::string_view file, std::string_view filename) final {

  }

};
namespace style {

static constexpr std::string_view bold = "\e[1m";
static constexpr std::string_view clear = "\e[0m";

struct none {
  static constexpr std::string_view name = "";
  static constexpr std::string_view escape = "\e[0m";
};

struct error {
  static constexpr std::string_view name = "error";
  static constexpr std::string_view escape = "\e[31m";
};

struct note {
  static constexpr std::string_view name = "note";
  static constexpr std::string_view escape = "\e[36m";
};

struct warning {
  static constexpr std::string_view name = "warning";
  static constexpr std::string_view escape = "\e[35m";
};

}

template<typename Style>
struct styled_string : public virtual base {
  std::string_view what;
  styled_string(std::string_view what) : what(what) {}
  void print(std::ostream &os) const final {
    os << Style::escape << Style::name << ": " << Style::none::escape << what << std::endl;
  }
  void link_file(std::string_view file, std::string_view filename) final {
  }

};
typedef styled_string<style::error> error_string;
typedef styled_string<style::warning> warning_string;
typedef styled_string<style::note> note_string;

template<typename Style>
struct report_token : public virtual base {

  std::string_view token, file, filename;
  report_token(std::string_view token, std::string_view file, std::string_view filename)
      : token(token), file(file), filename(filename) {}
  report_token(std::string_view token)
      : token(token) {}
  void link_file(std::string_view file, std::string_view filename) final {
    if (token.begin() >= file.begin() && token.end() <= file.end()) {
      this->file = file;
      this->filename = filename;
    }
  }

  virtual void describe(std::ostream &os) const = 0;
  void print(std::ostream &os) const final {
    constexpr std::string_view bold_style = "\e[1m";
    constexpr std::string_view clear_style = "\e[0m";
    constexpr std::string_view sep = ":";
    constexpr std::string_view ssep = ": ";
    std::string_view tk = token;
    std::size_t posy, posx;
    os << bold_style;
    if (!file.empty() && tk.begin() >= file.begin() && tk.end() <= file.end()) {
      posy = std::count(file.begin(), tk.begin(), 10) + 1;
      posx = std::distance(std::find(std::string_view::const_reverse_iterator(tk.begin() + 1), file.crend(), 10).base(),
                           tk.begin()) + 1;
      os << filename << sep << posy << sep << posx << ssep;
    }
    os << Style::escape << Style::name << ssep << clear_style;
    describe(os);
    os << std::endl;
    if (!file.empty() && tk.begin() >= file.begin() && tk.end() <= file.end()) {

      auto it_endline = tk.begin();
      if (auto ite = std::find(tk.begin(), tk.end(), 10);ite != tk.end()) {
        it_endline = ite;
        tk.remove_suffix(std::distance(ite, tk.end()));
      } else {
        it_endline = std::find(tk.end(), file.end(), 10);
      }
      for (int w = int(5) - std::to_string(posy).size(); w > 0; w--)os << ' ';
      os << posy << " | " << itr_sv(tk.begin() - posx + 1, tk.begin()) << bold_style << Style::escape << tk
         << clear_style << itr_sv(tk.end(), it_endline) << std::endl;
      os << "      |" << std::string(posx, ' ') << bold_style << Style::escape << "^";
      for (int w = int(tk.size()) - 1; w > 0; w--)os << '~';
      os << clear_style << std::endl;
    }
  }
}; // virtual class
typedef report_token<style::error> error_token;
typedef report_token<style::warning> warning_token;
typedef report_token<style::note> note_token;

template<typename Style>
struct report_token_front_back : public report_token<Style> {
  std::string_view front, back;
  typedef report_token<Style> base_rt;
  report_token_front_back(std::string_view front,
                          std::string_view token,
                          std::string_view back,
                          std::string_view file,
                          std::string_view filename) : base_rt(token, file, filename), front(front), back(back) {}
  void describe(std::ostream &os) const final {
    os << front;
    if (!front.empty() && front.back() != ' ')os << " ";
    os << style::bold << base_rt::token << style::clear;
    if (!back.empty() && back.front() != ' ')os << " ";
    os << back;
  }
};
typedef report_token_front_back<style::error> error_report_token_front_back;
typedef report_token_front_back<style::warning> warning_report_token_front_back;
typedef report_token_front_back<style::note> note_report_token_front_back;

}
#endif //COMPILERS_BML_LIB_ERRMSG_H_
