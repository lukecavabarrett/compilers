#ifndef COMPILERS_BML_LIB_AST_H_
#define COMPILERS_BML_LIB_AST_H_
#include <memory>
#include <vector>
#include <variant>

namespace ast {

namespace {
std::string char_to_html(char c) {
  if (c == ' ')return "&nbsp;";
  if (c == '\n')return "<br>";
  return std::string{c};
}
}

struct code_piece {
  std::string_view loc;
  explicit code_piece(std::string_view loc);
  virtual std::string html_description() const = 0;
  virtual void _make_html_childcall(std::string &out, std::string_view::iterator &it) const = 0;
  void make_html(std::string &out, std::string_view::iterator &it) const;;
  std::string to_html() const;

};

//Base definitions
namespace expression {
struct t : public code_piece {
  using code_piece::code_piece;
};
typedef std::unique_ptr<t> ptr;
}

namespace matcher {
struct t : public code_piece {
  using code_piece::code_piece;
};
typedef std::unique_ptr<t> ptr;
}

namespace definition {

struct single : code_piece {
  typedef std::unique_ptr<single> ptr;
  expression::ptr body;
  single(expression::ptr &&b, std::string_view l) : code_piece(l), body(std::move(b)) {}
};

struct t : code_piece {
  bool rec = false;
  std::vector<single::ptr> defs;
  std::string html_description() const final { return "Definition(s)"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    for (const auto &p : defs)p->make_html(out, it);
  };
  t() : code_piece(std::string_view()) {};
};
typedef std::unique_ptr<t> ptr;

}

namespace literal {
struct t  {
  virtual std::string html_description() const = 0;
};
typedef std::unique_ptr<t> ptr;
}

//Extended definitions

namespace expression {

struct literal : public t {
  typedef std::unique_ptr<literal> ptr;
  std::string html_description() const final { return value->html_description(); }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {};
  ast::literal::ptr value;
  literal(ast::literal::ptr&& v,std::string_view loc  ) : t(loc), value(std::move(v)) {}
};


struct identifier : public t {
  std::string html_description() const final { return "Identifier"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {};

  //const universal_bind &definition_point;
  using t::t;
};

struct if_then_else : public t {
  std::string html_description() const final { return "If_then_else"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    condition->make_html(out, it);
    true_branch->make_html(out, it);
    false_branch->make_html(out, it);
  };

  expression::ptr condition, true_branch, false_branch;
  if_then_else(expression::ptr &&condition, expression::ptr &&true_branch, expression::ptr &&false_branch, std::string_view loc)
      : t(loc), condition(std::move(condition)), true_branch(std::move(true_branch)), false_branch(std::move(false_branch)) {}
};

struct build_tuple : public t {
  std::string html_description() const final { return "Build_tuple"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    for (const auto &p : args)p->make_html(out, it);
  };
  std::vector<expression::ptr> args;
  build_tuple(std::vector<expression::ptr> &&args, std::string_view loc) : t(loc), args(std::move(args)) {}
  build_tuple(std::string_view loc) : t(loc) {}
};

struct fun_app : public t {
  std::string html_description() const final { return "Function application"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    f->make_html(out, it);
    x->make_html(out, it);
  };
  expression::ptr f, x;
  fun_app(expression::ptr &&f, expression::ptr &&x, std::string_view loc) : t(loc), f(std::move(f)), x(std::move(x)) {}

};

struct seq : public t {
  std::string html_description() const final { return "Sequence"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    a->make_html(out, it);
    b->make_html(out, it);
  };
  expression::ptr a, b;
  seq(expression::ptr &&a, expression::ptr &&b, std::string_view loc) : t(loc), a(std::move(a)), b(std::move(b)) {}

};

struct match_with : public t {
  std::string html_description() const final { return "Match_with"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    what->make_html(out, it);
    for (const auto &[p, r] : branches) {
      p->make_html(out, it);
      r->make_html(out, it);
    }
  };
  typedef std::unique_ptr<match_with> ptr;
  struct branch {
    matcher::ptr pattern;
    expression::ptr result;
  };
  expression::ptr what;
  std::vector<branch> branches;
  match_with(expression::ptr &&w, std::string_view loc) : t(loc), what(std::move(w)) {}

};

struct let_in : public t {
  std::string html_description() const final { return "Let_in"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    d->make_html(out, it);
    e->make_html(out, it);
  };
  definition::ptr d;
  expression::ptr e;
  let_in(definition::ptr &&d, expression::ptr &&e, std::string_view loc) : t(loc), d(std::move(d)), e(std::move(e)) {}

};

}

namespace matcher {

struct universal_matcher : public t {
  std::string html_description() const final { return "Universal matcher"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {};
  typedef std::unique_ptr<universal_matcher> ptr;
  using t::t;
};

struct anonymous_universal_matcher : public t {
  std::string html_description() const final { return "Ignore matcher"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {};
  typedef std::unique_ptr<anonymous_universal_matcher> ptr;
  using t::t;
};

struct constructor_matcher : public t {
  std::string html_description() const final { return "Constructor matcher"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    if (arg)arg->make_html(out, it);
  };
  typedef std::unique_ptr<constructor_matcher> ptr;
  matcher::ptr arg;
  std::string_view cons;
  constructor_matcher(matcher::ptr &&m, std::string_view loc, std::string_view c) : t(loc), arg(std::move(m)), cons(c) {}
  constructor_matcher(std::string_view loc) : t(loc), arg(), cons(loc) {}

};

struct literal_matcher : public t {
  std::string html_description() const final { return "Literal matcher"; }

  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {};
  typedef std::unique_ptr<literal_matcher> ptr;
  ast::literal::ptr value;
  literal_matcher(ast::literal::ptr &&lit, std::string_view loc) : t(loc), value(std::move(lit)) {}
};

struct tuple_matcher : public t {
  std::string html_description() const final { return "Match tuple"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    for (const auto &p : args)p->make_html(out, it);
  };
  std::vector<matcher::ptr> args;
  tuple_matcher(std::vector<matcher::ptr> &&args, std::string_view loc) : t(loc), args(std::move(args)) {}
  tuple_matcher(std::string_view loc) : t(loc) {}
};

}

namespace definition {

struct function : single {
  typedef std::unique_ptr<function> ptr;
  std::string html_description() const final { return "Function definition"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    name->make_html(out, it);
    for (const auto &p : args)p->make_html(out, it);
    body->make_html(out, it);
  };
  matcher::universal_matcher::ptr name;
  std::vector<matcher::ptr> args;
  using single::single;
  function() : single(expression::ptr(), std::string_view()) {}
};

struct value : single {
  typedef std::unique_ptr<value> ptr;
  std::string html_description() const final { return "Value binding"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    binded->make_html(out, it);
    body->make_html(out, it);
  };
  matcher::ptr binded;
  value(matcher::ptr &&bi, expression::ptr &&bo, std::string_view loc) : single(std::move(bo), loc), binded(std::move(bi)) {}
};

}

namespace literal {

struct integer : public t {
  std::string html_description() const final { return "Int Literal"; }

  int64_t value;
  integer(int64_t value) : value(value) {}
};
struct boolean : public t {
  std::string html_description() const final { return "Bool Literal"; }

  bool value;
  boolean(bool value) : value(value) {}
};
struct unit : public t {
  std::string html_description() const final { return "Unit Literal"; }
  unit() {}
};
struct string : public t {
  std::string html_description() const final { return "String Literal"; }
  std::string value;
  string(std::string_view value) : value(value) {}
};
}

}

#endif //COMPILERS_BML_LIB_AST_H_
