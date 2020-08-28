#ifndef COMPILERS_BML_LIB_AST_H_
#define COMPILERS_BML_LIB_AST_H_
#include <memory>
#include <vector>
#include <variant>
#include <bind.h>
#include "util.h"

namespace ast {

namespace {
std::string char_to_html(char c) {
  if (c == ' ')return "&nbsp;";
  if (c == '\n')return "<br>";
  return std::string{c};
}
}//Forward
namespace matcher {
struct t;
typedef std::unique_ptr<t> ptr;
struct universal_matcher;
}

typedef bind::name_table<ast::matcher::universal_matcher> ltable;

struct node {
  std::string_view loc;
  explicit node(std::string_view loc);
  virtual std::string html_description() const = 0;
  virtual void _make_html_childcall(std::string &out, std::string_view::iterator &it) const = 0;
  void make_html(std::string &out, std::string_view::iterator &it) const;;
  std::string to_html() const;

};



//Base definitions
namespace expression {
struct t : public node {
  using node::node;
  virtual void bind(const ltable &) = 0;
};
typedef std::unique_ptr<t> ptr;
}

//Base definitions
namespace matcher {
struct t : public node {
  using node::node;
  virtual void bind(ltable::map_t &) = 0;
};
typedef std::unique_ptr<t> ptr;
struct universal_matcher;
}

namespace definition {

struct single : node {
  typedef std::unique_ptr<single> ptr;
  expression::ptr body;
  single(expression::ptr &&b, std::string_view l) : node(l), body(std::move(b)) {}
  virtual void bind(const ltable &) = 0;;
  virtual void bind(ltable::map_t &mt) = 0;;
};

struct t : node {
  bool rec = false;
  std::vector<single::ptr> defs;
  std::string html_description() const final { return "Definition(s)"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    for (const auto &p : defs)p->make_html(out, it);
  };

  t() : node(std::string_view()) {};
  ltable bind(const ltable &);;
};
typedef std::unique_ptr<t> ptr;

}

namespace literal {
struct t {
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
  literal(ast::literal::ptr &&v, std::string_view loc) : t(loc), value(std::move(v)) {}
  void bind(const ltable &) final {}
};

struct identifier : public t {
  std::string html_description() const final { return "Identifier"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {};
  void bind(const ltable &lt) final { definition_point = &lt.lookup(loc);}
  const matcher::universal_matcher *definition_point;
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
  void bind(const ltable &lt) final {
    condition->bind(lt);
    true_branch->bind(lt);
    false_branch->bind(lt);
  }
};

struct build_tuple : public t {
  std::string html_description() const final { return "Build_tuple"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    for (const auto &p : args)p->make_html(out, it);
  };
  std::vector<expression::ptr> args;
  build_tuple(std::vector<expression::ptr> &&args, std::string_view loc) : t(loc), args(std::move(args)) {}
  build_tuple(std::string_view loc) : t(loc) {}
  void bind(const ltable &lt) final { for (auto &p : args)p->bind(lt); }

};

struct fun_app : public t {
  std::string html_description() const final { return "Function application"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    f->make_html(out, it);
    x->make_html(out, it);
  };
  expression::ptr f, x;
  fun_app(expression::ptr &&f, expression::ptr &&x, std::string_view loc) : t(loc), f(std::move(f)), x(std::move(x)) {}
  void bind(const ltable &lt) final {
    f->bind(lt);
    x->bind(lt);
  }

};

struct seq : public t {
  std::string html_description() const final { return "Sequence"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    a->make_html(out, it);
    b->make_html(out, it);
  };
  expression::ptr a, b;
  seq(expression::ptr &&a, expression::ptr &&b, std::string_view loc) : t(loc), a(std::move(a)), b(std::move(b)) {}
  void bind(const ltable &lt) final {
    a->bind(lt);
    b->bind(lt);
  }

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
  void bind(const ltable &lt) final {
    what->bind(lt);
    for (auto&[p, r] : branches) {
      ltable wp = lt.sub_table();
      p->bind(wp.map());
      r->bind(wp);
    }
  }

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
  void bind(const ltable &lt) final {
    e->bind(d->bind(lt));
  }
};

}

namespace matcher {

struct universal_matcher : public t {
  std::string html_description() const final { return "Universal matcher"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {};
  typedef std::unique_ptr<universal_matcher> ptr;
  using t::t;
  void bind(ltable::map_t &m) final {
    m.bind(loc, *this);
  }
};

struct anonymous_universal_matcher : public t {
  std::string html_description() const final { return "Ignore matcher"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {};
  typedef std::unique_ptr<anonymous_universal_matcher> ptr;
  using t::t;
  void bind(ltable::map_t &m) final {}
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
  void bind(ltable::map_t &m) final {
    if (arg)arg->bind(m);
  }

};

struct literal_matcher : public t {
  std::string html_description() const final { return "Literal matcher"; }

  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {};
  typedef std::unique_ptr<literal_matcher> ptr;
  ast::literal::ptr value;
  literal_matcher(ast::literal::ptr &&lit, std::string_view loc) : t(loc), value(std::move(lit)) {}
  void bind(ltable::map_t &m) final {}
};

struct tuple_matcher : public t {
  std::string html_description() const final { return "Match tuple"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    for (const auto &p : args)p->make_html(out, it);
  };
  std::vector<matcher::ptr> args;
  tuple_matcher(std::vector<matcher::ptr> &&args, std::string_view loc) : t(loc), args(std::move(args)) {}
  tuple_matcher(std::string_view loc) : t(loc) {}
  void bind(ltable::map_t &m) final {
    for (auto &p : args)p->bind(m);
  }
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
  void bind(const ltable &lt) final {
    ltable argtable = lt.sub_table();
    for (auto &p : args)p->bind(argtable.map());
    body->bind(argtable);
  }
  void bind(ltable::map_t &mt) final {
    name->bind(mt);
  }
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
  void bind(const ltable &lt) final {
    body->bind(lt);
  }
  void bind(ltable::map_t &mt) final {
    binded->bind(mt);
  }
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

std::optional<
    std::pair<
          std::vector<
                std::variant<definition::ptr,
                            expression::ptr>
                > ,
          ltable>
    > compile(std::string_view source,const ltable& lt,std::string_view filename = "source");

}

#endif //COMPILERS_BML_LIB_AST_H_
