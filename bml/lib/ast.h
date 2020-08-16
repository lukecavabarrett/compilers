#ifndef COMPILERS_BML_LIB_AST_H_
#define COMPILERS_BML_LIB_AST_H_
#include <memory>
#include <vector>
#include <variant>

namespace ast {

namespace {
std::string char_to_html(char c) {
  return std::string{c};
}
}

struct code_piece {
  std::string_view loc;
  code_piece(std::string_view loc) : loc(loc) {}
  virtual std::string _make_html_label() const = 0;
  virtual void _make_html_childcall(std::string &out, std::string_view::iterator &it) const = 0;
  void make_html(std::string &out, std::string_view::iterator &it) const {
    while (it < loc.begin()) {
      out.append(char_to_html(*it));
      ++it;
    }
    out.append(R"(<span class="box" title=")").append(_make_html_label()).append("\">");
    _make_html_childcall(out, it);
    while (it < loc.end()) {
      out.append(char_to_html(*it));
      ++it;
    }
    out.append("</span>");
  };
  std::string to_html() const {
    std::string out;
    auto it = loc.begin();
    make_html(out, it);
    return out;
  }

};

struct expression : public code_piece {
  typedef std::unique_ptr<expression> ptr;
  using code_piece::code_piece;
};

struct universal_bind;
struct matcher : public code_piece {
  typedef std::unique_ptr<matcher> ptr;

  // UniversalIgnore | UniversalBind of string | Literal of Literal.t | Tuple of t list | Constructor of string * (t option)

};

struct universal_bind : public matcher {
  typedef std::unique_ptr<universal_bind> ptr;
};

struct single_definition : code_piece {
  typedef std::unique_ptr<single_definition> ptr;
  expression::ptr body;
};

struct function_definition : single_definition {
  universal_bind::ptr name;
  std::vector<matcher::ptr> args;

};

struct value_definition : single_definition {
  matcher::ptr binded;
};

struct definition : code_piece {
  bool rec = false;
  std::vector<single_definition::ptr> defs;
  typedef std::unique_ptr<definition> ptr;

};

struct literal : public expression {
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {};
  using expression::expression;
};
struct int_literal : public literal {
  std::string _make_html_label() const final { return "Int Literal"; }

  int64_t value;
  int_literal(int64_t value, std::string_view loc) : literal(loc), value(value) {}
};
struct bool_literal : public literal {
  std::string _make_html_label() const final { return "Bool Literal"; }

  bool value;
  bool_literal(bool value, std::string_view loc) : literal(loc), value(value) {}
};
struct unit_literal : public literal {
  using literal::literal;
  std::string _make_html_label() const final { return "Unit Literal"; }
};

struct identifier : public expression {
  std::string _make_html_label() const final {return "Identifier";}
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {};

  //const universal_bind &definition_point;
  using expression::expression;
};

struct if_then_else : public expression {
  std::string _make_html_label() const final {return "If_then_else";}
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    condition->make_html(out,it);
    true_branch->make_html(out,it);
    false_branch->make_html(out,it);
  };

  expression::ptr condition, true_branch, false_branch;
  if_then_else(expression::ptr &&condition, expression::ptr &&true_branch, expression::ptr &&false_branch, std::string_view loc)
      : expression(loc), condition(std::move(condition)), true_branch(std::move(true_branch)), false_branch(std::move(false_branch)) {}
};

struct build_tuple : public expression {
  std::string _make_html_label() const final {return "Build_tuple";}
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    for(const auto& p : args)p->make_html(out,it);
  };
  std::vector<expression::ptr> args;
  build_tuple(std::vector<expression::ptr> &&args, std::string_view loc) : expression(loc), args(std::move(args)) {}
  build_tuple(std::string_view loc) : expression(loc) {}
};

struct fun_app : public expression {
  std::string _make_html_label() const final {return "Function application";}
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    f->make_html(out,it);
    x->make_html(out,it);
  };
  expression::ptr f, x;
  fun_app(expression::ptr &&f, expression::ptr &&x, std::string_view loc) : expression(loc), f(std::move(f)), x(std::move(x)) {}

};

struct seq : public expression {
  std::string _make_html_label() const final {return "Sequence";}
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    a->make_html(out,it);
    b->make_html(out,it);
  };
  expression::ptr a, b;
  seq(expression::ptr &&a, expression::ptr &&b, std::string_view loc) : expression(loc), a(std::move(a)), b(std::move(b)) {}

};

struct let_in : public expression {

};

}

#endif //COMPILERS_BML_LIB_AST_H_
