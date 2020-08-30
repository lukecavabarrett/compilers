#ifndef COMPILERS_BML_LIB_AST_H_
#define COMPILERS_BML_LIB_AST_H_
#include <memory>
#include <vector>
#include <variant>
#include <bind.h>
#include <util/util.h>
#include <util/sexp.h>

namespace ast {
using namespace util;

namespace {
typedef sexp::sexp_of_t sexp_of_t;
std::string char_to_html(char c) {
  if (c == ' ')return "&nbsp;";
  if (c == '\n')return "<br>";
  return std::string{c};
}

template<typename P1, typename P2>
std::string_view unite_sv(const P1 &p1, const P2 &p2) {
  return itr_sv(p1->loc.begin(), p2->loc.end());
}

}//Forward
namespace matcher {
struct t;
typedef std::unique_ptr<t> ptr;
struct universal_matcher;
}

typedef bind::name_table<ast::matcher::universal_matcher> ltable;

struct locable {
  std::string_view loc;
  virtual std::string html_description() const { THROW_UNIMPLEMENTED }
  virtual void _make_html_childcall(std::string &out, std::string_view::iterator &it) const { THROW_UNIMPLEMENTED }
  void make_html(std::string &out, std::string_view::iterator &it) const;;
  std::string to_html() const;

};



//Base definitions
namespace expression {
struct t : public locable, public sexp_of_t {
  using locable::locable;
  virtual void bind(const ltable &) = 0;
};
typedef std::unique_ptr<t> ptr;
}

namespace matcher {
struct t : public locable, sexp_of_t {
  virtual void bind(ltable::map_t &) = 0;
};
typedef std::unique_ptr<t> ptr;
struct universal_matcher;
}

namespace definition {

struct single : public locable,sexp_of_t {
  typedef std::unique_ptr<single> ptr;
  expression::ptr body;
  single(expression::ptr &&b) : body(std::move(b)) {}
  virtual void bind(const ltable &) = 0;;
  virtual void bind(ltable::map_t &mt) = 0;;
};

struct t : locable,sexp_of_t {
  bool rec = false;
  std::vector<single::ptr> defs;
  std::string html_description() const final { return "Definition(s)"; }
  ltable bind(const ltable &);;
  TO_SEXP(rec,defs);
};
typedef std::unique_ptr<t> ptr;

}

namespace literal {
struct t : public sexp_of_t {
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
  literal(ast::literal::ptr &&v) : value(std::move(v)) {}
  void bind(const ltable &) final {}

  TO_SEXP(value);
};

struct identifier : public t {
  std::string html_description() const final { return "Identifier"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {};
  void bind(const ltable &lt) final { definition_point = &lt.lookup(name); }
  const matcher::universal_matcher *definition_point;
  identifier(std::string_view n) : name(n) {}
  std::string_view name;
  TO_SEXP(name);
};

struct if_then_else : public t {
  std::string html_description() const final { return "If_then_else"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    condition->make_html(out, it);
    true_branch->make_html(out, it);
    false_branch->make_html(out, it);
  };

  expression::ptr condition, true_branch, false_branch;
  if_then_else(expression::ptr &&condition, expression::ptr &&true_branch, expression::ptr &&false_branch)
      : condition(std::move(condition)), true_branch(std::move(true_branch)), false_branch(std::move(false_branch)) {}
  void bind(const ltable &lt) final {
    condition->bind(lt);
    true_branch->bind(lt);
    false_branch->bind(lt);
  }
  TO_SEXP(condition, true_branch, false_branch);
};

struct build_tuple : public t {
  std::string html_description() const final { return "Build_tuple"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    for (const auto &p : args)p->make_html(out, it);
  };
  std::vector<expression::ptr> args;
  build_tuple(std::vector<expression::ptr> &&args) : args(std::move(args)) {}
  build_tuple() = default;
  void bind(const ltable &lt) final { for (auto &p : args)p->bind(lt); }
  TO_SEXP(args);
};

struct fun_app : public t {
  std::string html_description() const final { return "Function application"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    f->make_html(out, it);
    x->make_html(out, it);
  };
  expression::ptr f, x;
  fun_app(expression::ptr &&f_, expression::ptr &&x_) : f(std::move(f_)), x(std::move(x_)) {
    loc = unite_sv(f, x);
  }
  void bind(const ltable &lt) final {
    f->bind(lt);
    x->bind(lt);
  }
  TO_SEXP(f, x);
};

struct seq : public t {
  std::string html_description() const final { return "Sequence"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    a->make_html(out, it);
    b->make_html(out, it);
  };
  expression::ptr a, b;
  seq(expression::ptr &&a, expression::ptr &&b) : a(std::move(a)), b(std::move(b)) { loc = unite_sv(this->a, this->b); }
  void bind(const ltable &lt) final {
    a->bind(lt);
    b->bind(lt);
  }
  TO_SEXP(a, b);
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
  struct branch : public sexp_of_t {
    matcher::ptr pattern;
    expression::ptr result;
    TO_SEXP(pattern,result)
  };
  expression::ptr what;
  std::vector<branch> branches;
  match_with(expression::ptr &&w) : what(std::move(w)) {}
  void bind(const ltable &lt) final {
    what->bind(lt);
    for (auto&[p, r] : branches) {
      ltable wp = lt.sub_table();
      p->bind(wp.map());
      r->bind(wp);
    }
  }
  TO_SEXP(what,branches);
};

struct let_in : public t {
  std::string html_description() const final { return "Let_in"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    d->make_html(out, it);
    e->make_html(out, it);
  };
  definition::ptr d;
  expression::ptr e;
  let_in(definition::ptr &&d, expression::ptr &&e) : d(std::move(d)), e(std::move(e)) {}
  void bind(const ltable &lt) final {
    e->bind(d->bind(lt));
  }
  TO_SEXP(d,e);
};

}

namespace matcher {

struct universal_matcher : public t {
  std::string html_description() const final { return "Universal matcher"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {};
  typedef std::unique_ptr<universal_matcher> ptr;
  universal_matcher(std::string_view n) : name(n) {}
  void bind(ltable::map_t &m) final {
    m.bind(loc, *this);
  }
  std::string_view name;
  TO_SEXP(name)
};

struct anonymous_universal_matcher : public t {
  std::string html_description() const final { return "Ignore matcher"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {};
  typedef std::unique_ptr<anonymous_universal_matcher> ptr;
  void bind(ltable::map_t &m) final {}
  TO_SEXP()
};

struct constructor_matcher : public t {
  std::string html_description() const final { return "Constructor matcher"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    if (arg)arg->make_html(out, it);
  };
  typedef std::unique_ptr<constructor_matcher> ptr;
  matcher::ptr arg;
  std::string_view cons;
  constructor_matcher(matcher::ptr &&m, std::string_view c) : arg(std::move(m)), cons(c) {}
  constructor_matcher(std::string_view c) : arg(), cons(c) {}
  void bind(ltable::map_t &m) final {
    if (arg)arg->bind(m);
  }
  TO_SEXP(cons,arg); //TODO: arg is optional expect SEGFAULT
};

struct literal_matcher : public t {
  std::string html_description() const final { return "Literal matcher"; }

  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {};
  typedef std::unique_ptr<literal_matcher> ptr;
  ast::literal::ptr value;
  literal_matcher(ast::literal::ptr &&lit) : value(std::move(lit)) {}
  void bind(ltable::map_t &m) final {}
  TO_SEXP(value);
};

struct tuple_matcher : public t {
  std::string html_description() const final { return "Match tuple"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    for (const auto &p : args)p->make_html(out, it);
  };
  std::vector<matcher::ptr> args;
  tuple_matcher() = default;
  void bind(ltable::map_t &m) final {
    for (auto &p : args)p->bind(m);
  }
  TO_SEXP(args);
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
  function() : single(expression::ptr()) {}
  void bind(const ltable &lt) final {
    ltable argtable = lt.sub_table();
    for (auto &p : args)p->bind(argtable.map());
    body->bind(argtable);
  }
  void bind(ltable::map_t &mt) final {
    name->bind(mt);
  }
  TO_SEXP(name,args,body)
};

struct value : single {
  typedef std::unique_ptr<value> ptr;
  std::string html_description() const final { return "Value binding"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    binded->make_html(out, it);
    body->make_html(out, it);
  };
  matcher::ptr binded;
  value(matcher::ptr &&bi, expression::ptr &&bo) : single(std::move(bo)), binded(std::move(bi)) {}
  void bind(const ltable &lt) final {
    body->bind(lt);
  }
  void bind(ltable::map_t &mt) final {
    binded->bind(mt);
  }
  TO_SEXP(binded,body)
};

}

namespace literal {

struct integer : public t {
  std::string html_description() const final { return "Int Literal"; }

  int64_t value;
  integer(int64_t value) : value(value) {}
  TO_SEXP(value)
};
struct boolean : public t {
  std::string html_description() const final { return "Bool Literal"; }

  bool value;
  boolean(bool value) : value(value) {}
  TO_SEXP(value)
};
struct unit : public t {
  std::string html_description() const final { return "Unit Literal"; }
  unit() {}
  TO_SEXP("()")
};
struct string : public t {
  std::string html_description() const final { return "String Literal"; }
  std::string value;
  string(std::string_view value) : value(value) {}
  TO_SEXP(value)
};
}

namespace type {

namespace expression {

struct t : public locable, public sexp_of_t {
};
typedef std::unique_ptr<t> ptr;

struct identifier : public t {
  typedef std::unique_ptr<identifier> ptr;
  std::string_view name;
  identifier(std::string_view s) : name(s) {}
  TO_SEXP(name);
}; //e.g. 'a or int
struct function : public t {
  ptr from, to;
  function(ptr &&f, ptr &&x) : from(std::move(f)), to(std::move(x)) { loc = unite_sv(from, to); }
  TO_SEXP(from, to);
};
struct product : public t {
  typedef std::unique_ptr<product> ptr;

  std::vector<expression::ptr> ts; //size>=2
  //void set_loc() {loc = itr_sv(ts.front()->loc.begin(),ts.back()->loc.end());}
  TO_SEXP(ts);
};
struct tuple : public t {
  typedef std::unique_ptr<tuple> ptr;

  std::vector<expression::ptr> ts; //size>=2
  tuple(expression::ptr &&x) { ts.push_back(std::move(x)); }
  //void set_loc() {loc = itr_sv(ts.front()->loc.begin(),ts.back()->loc.end());}
  TO_SEXP(ts);
};
struct constr : public t {
  ptr x, f;
  constr(ptr &&xx, ptr &&ff) : x(std::move(xx)), f(std::move(ff)) { loc = itr_sv(x->loc.begin(), f->loc.end()); }
  TO_SEXP(x, f)
};

}

namespace definition {

struct param : public locable, public sexp_of_t {
  typedef std::unique_ptr<param> ptr;
  param(std::string_view s) : name(s) {}
  std::string_view name;
  TO_SEXP(name);
};

struct single : public locable, public sexp_of_t {
  typedef std::unique_ptr<single> ptr;
  std::vector<param::ptr> params;
  std::string_view name;
};

struct t : public locable, public sexp_of_t {
  bool nonrec = false;
  std::vector<single::ptr> defs;
  TO_SEXP(nonrec, defs);
};
typedef std::unique_ptr<t> ptr;

struct single_texpr : public single {
  typedef std::unique_ptr<single_texpr> ptr;

  expression::ptr type;
  TO_SEXP(name, type);
};

struct single_variant : public single {
  typedef std::unique_ptr<single_variant> ptr;

  struct constr : public sexp_of_t {
    std::string_view name;
    expression::ptr type;
    TO_SEXP(name, type);
  };

  std::vector<constr> variants;
  TO_SEXP(name, variants);

};

}

}

}

#endif //COMPILERS_BML_LIB_AST_H_
