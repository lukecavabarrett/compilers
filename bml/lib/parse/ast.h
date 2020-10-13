#ifndef COMPILERS_BML_LIB_AST_H_
#define COMPILERS_BML_LIB_AST_H_
#include <memory>
#include <vector>
#include <variant>
#include <bind.h>
#include <util/util.h>
#include <util/sexp.h>
#include <forward_list>
#include <unordered_set>
#include <cinttypes>

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

namespace expression {
struct identifier;
}

typedef std::forward_list<expression::identifier *> usage_list;
typedef std::unordered_map<std::string_view, usage_list> free_vars_t;
typedef std::unordered_set<const matcher::universal_matcher *> capture_set;
namespace {
free_vars_t join_free_vars(free_vars_t &&v1, free_vars_t &&v2) {
  if (v1.size() < v2.size())std::swap(v1, v2);
  for (auto&[k, l] : v2) {
    if (auto it = v1.find(k);it == v1.end()) {
      v1.try_emplace(k, std::move(l));
    } else {
      it->second.splice_after(it->second.cbegin(), l, l.cbefore_begin());
    }
  }
  return v1;
}
capture_set join_capture_set(capture_set &&c1, capture_set &&c2) {
  if (c1.size() < c2.size())std::swap(c1, c2);
  c1.merge(std::move(c2));
  return c1;
}
}

typedef bind::name_table<ast::matcher::universal_matcher> ltable;

struct locable {
  std::string_view loc;
  virtual std::string html_description() const { THROW_UNIMPLEMENTED }
  virtual void _make_html_childcall(std::string &out, std::string_view::iterator &it) const { THROW_UNIMPLEMENTED }
  void make_html(std::string &out, std::string_view::iterator &it) const;;
  std::string to_html() const;
  virtual ~locable() = default;
};



//Base definitions
namespace expression {
struct t : public locable, public sexp_of_t {
  using locable::locable;
  virtual free_vars_t free_vars() = 0;
  virtual capture_set capture_group() = 0;
};
typedef std::unique_ptr<t> ptr;
}

namespace matcher {
struct t : public locable, sexp_of_t {
  virtual void bind(free_vars_t &) = 0;
  virtual void bind(capture_set &) = 0;
};
typedef std::unique_ptr<t> ptr;
struct universal_matcher;
}

namespace definition {

struct single : public locable, sexp_of_t {
  typedef std::unique_ptr<single> ptr;
  expression::ptr body;
  single(expression::ptr &&b) : body(std::move(b)) {}
  virtual matcher::t &binder() const = 0;
  virtual free_vars_t free_vars() = 0;
  virtual capture_set capture_group() = 0;
  virtual ~single() = default;
};

struct function;

struct t : locable, sexp_of_t {
  bool rec = false;
  std::vector<single::ptr> defs;
  std::string html_description() const final { return "Definition(s)"; }
  free_vars_t free_vars(free_vars_t &&fv = {}) {
    if (rec) {
      for (auto &def : defs)fv = join_free_vars(std::move(fv), def->free_vars());
      for (auto &def : defs)def->binder().bind(fv);
    } else {
      for (auto &def : defs)def->binder().bind(fv);
      for (auto &def : defs)fv = join_free_vars(std::move(fv), def->free_vars());
    }
    return fv;
  }
  capture_set capture_group(capture_set &&cs = {}) {
    if (rec) {
      for (auto &def : defs)cs = join_capture_set(std::move(cs), def->capture_group());
      for (auto &def : defs)def->binder().bind(cs);
    } else {
      for (auto &def : defs)def->binder().bind(cs);
      for (auto &def : defs)cs = join_capture_set(std::move(cs), def->capture_group());
    }
    return cs;
  }
  TO_SEXP(rec, defs);
};
typedef std::unique_ptr<t> ptr;

}

namespace literal {
struct t : public sexp_of_t {
  virtual std::string html_description() const = 0;
  virtual ~t() = default;
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
  free_vars_t free_vars() final { return {}; };
  capture_set capture_group() final { return {}; };

  TO_SEXP(value);
};

struct identifier : public t {
  std::string html_description() const final { return "Identifier"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {};
  free_vars_t free_vars() final;
  const matcher::universal_matcher *definition_point;
  const definition::function *closured; // If it's nullptr it means it have direct access to it
  identifier(std::string_view n) : name(n), definition_point(nullptr) {}
  capture_set capture_group() final;
  std::string_view name;
  TO_SEXP(name);
};

struct constructor : public t {
  std::string html_description() const final { return "Constructor"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {};
  free_vars_t free_vars() final { return {}; };
  capture_set capture_group() final { return {}; };
  constructor(std::string_view n) : name(n) {}
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
  free_vars_t free_vars() final { return join_free_vars(join_free_vars(condition->free_vars(), true_branch->free_vars()), false_branch->free_vars()); };
  capture_set capture_group() final { return join_capture_set(join_capture_set(condition->capture_group(), true_branch->capture_group()), false_branch->capture_group()); };

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
  free_vars_t free_vars() final {
    free_vars_t fv;
    for (auto &p : args)fv = join_free_vars(p->free_vars(), std::move(fv));
    return fv;
  }
  capture_set capture_group() final {
    capture_set fv;
    for (auto &p : args)fv = join_capture_set(p->capture_group(), std::move(fv));
    return fv;
  };

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
  free_vars_t free_vars() final { return join_free_vars(f->free_vars(), x->free_vars()); }
  capture_set capture_group() final { return join_capture_set(f->capture_group(), x->capture_group()); }
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
  free_vars_t free_vars() final { return join_free_vars(a->free_vars(), b->free_vars()); };
  capture_set capture_group() final { return join_capture_set(a->capture_group(), b->capture_group()); }

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
    TO_SEXP(pattern, result)
  };
  expression::ptr what;
  std::vector<branch> branches;
  match_with(expression::ptr &&w) : what(std::move(w)) {}
  free_vars_t free_vars() final {
    free_vars_t fv = what->free_vars();
    for (auto&[p, r] : branches) {
      free_vars_t bfv = r->free_vars();
      p->bind(bfv);
      fv = join_free_vars(std::move(fv), std::move(bfv));
    }
    return fv;
  };

  capture_set capture_group() final {
    capture_set cs = what->capture_group();
    for (auto&[p, r] : branches) {
      capture_set bcs = r->capture_group();
      p->bind(bcs);
      cs = join_capture_set(std::move(cs), std::move(bcs));
    }
    return cs;
  }

  TO_SEXP(what, branches);
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
  free_vars_t free_vars() final {
    return d->free_vars(e->free_vars());
  };
  capture_set capture_group() final {
    return d->capture_group(e->capture_group());
  };
  TO_SEXP(d, e);
};

}

namespace matcher {

struct universal_matcher : public t {
  std::string html_description() const final { return "Universal matcher"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {};
  typedef std::unique_ptr<universal_matcher> ptr;
  bool top_level =
      false; // True only for toplevel definition - this means
  // 1. It has a static address which doesn't expire
  // 2. Hence closures do not need to capture this
  // 3. It's type doesn't get changed by type inference - unified on a "per use" basis
  universal_matcher(std::string_view n) : name(n), top_level(false) {}
  void bind(free_vars_t &fv) final {
    if (auto it = fv.find(name); it != fv.end()) {
      usages = std::move(it->second);
      fv.erase(it);
      for (auto i : usages)i->definition_point = this;
    } else {
      //TODO-someday: warning that the name is unused
    }
  }
  void bind(capture_set &cs) final {
    cs.erase(this);
  }
  std::string_view name;
  usage_list usages;
  TO_SEXP(name)
};

struct anonymous_universal_matcher : public t {
  std::string html_description() const final { return "Ignore matcher"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {};
  typedef std::unique_ptr<anonymous_universal_matcher> ptr;
  void bind(free_vars_t &fv) final {}
  void bind(capture_set &cs) final {}
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
  void bind(free_vars_t &fv) final {
    if (arg)arg->bind(fv);
  }
  void bind(capture_set &cs) final {
    if (arg)arg->bind(cs);
  }

  TO_SEXP(cons, arg); //TODO: arg is optional expect SEGFAULT
};

struct literal_matcher : public t {
  std::string html_description() const final { return "Literal matcher"; }

  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {};
  typedef std::unique_ptr<literal_matcher> ptr;
  ast::literal::ptr value;
  literal_matcher(ast::literal::ptr &&lit) : value(std::move(lit)) {}
  void bind(free_vars_t &fv) final {}
  void bind(capture_set &cs) final {}

  TO_SEXP(value);
};

struct tuple_matcher : public t {
  std::string html_description() const final { return "Match tuple"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    for (const auto &p : args)p->make_html(out, it);
  };
  std::vector<matcher::ptr> args;
  tuple_matcher() = default;
  void bind(free_vars_t &fv) final {
    for (auto &p : args)p->bind(fv);
  }
  void bind(capture_set &cs) final {
    for (auto &p : args)p->bind(cs);
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
  matcher::t &binder() const final { return *name.get(); }
  std::vector<matcher::ptr> args;
  std::vector<const matcher::universal_matcher*> captures;
  using single::single;
  function() : single(expression::ptr()) {}
  free_vars_t free_vars() final {
    free_vars_t fv = body->free_vars();
    for (auto &m : args)m->bind(fv);
    return fv;
  };
  capture_set capture_group() final {
    capture_set cs = body->capture_group();
    for (auto &m : args)m->bind(cs);
    captures.assign(cs.begin(),cs.end());
    std::sort(captures.begin(),captures.end());
    if(captures.size()==1 && captures.front()==name.get()){
      //TODO-somdeday: the function can be made pure
      // captures.clear();
      // NOTE: the function can be made pure
    }
    return cs;
  }

  TO_SEXP(name, args, captures, body)
};

struct value : single {
  typedef std::unique_ptr<value> ptr;
  std::string html_description() const final { return "Value binding"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    binded->make_html(out, it);
    body->make_html(out, it);
  };
  matcher::ptr binded;
  matcher::t &binder() const final { return *binded.get(); }

  value(matcher::ptr &&bi, expression::ptr &&bo) : single(std::move(bo)), binded(std::move(bi)) {}
  free_vars_t free_vars() final {
    return body->free_vars();
  };
  capture_set capture_group() final {
    return body->capture_group();
  }
  TO_SEXP(binded, body)
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

namespace top_level {

//values
//types
//constructors

}

}

#endif //COMPILERS_BML_LIB_AST_H_
