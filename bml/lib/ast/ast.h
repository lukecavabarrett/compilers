#ifndef COMPILERS_BML_LIB_AST_H_
#define COMPILERS_BML_LIB_AST_H_
#include <memory>
#include <vector>
#include <variant>
#include <util/util.h>
#include <util/message.h>
#include <util/texp.h>
#include <forward_list>
#include <unordered_set>
#include <cinttypes>
#include <cassert>
#include <ir/lang.h>
#include <ir/ir.h>
#include <types/types.h>

namespace ast {
using namespace util;
typedef texp::texp_of_t texp_of_t;

namespace {
template<typename P1, typename P2>
std::string_view unite_sv(const P1 &p1, const P2 &p2) {
  return itr_sv(p1->loc.begin(), p2->loc.end());
}

}

namespace error {
class base : public std::runtime_error {
public:
  base() : std::runtime_error("ast::error") {}
};

class unused_value : public util::message::warning_token {
public:
  unused_value(std::string_view value)
      : util::message::warning_token(value) {}
  void describe(std::ostream &os) const {
    os << "value " << util::message::style::bold << token << util::message::style::clear
       << " is unused";
  }
};
class unbound_value : public base, public util::message::error_token {
public:
  unbound_value(std::string_view value)
      : util::message::error_token(value) {}
  void describe(std::ostream &os) const {
    os << "unbound value  " << util::message::style::bold << token << util::message::style::clear
       << " (maybe you forget a rec or you mispelt something) ";
  }
};

class unbound_constructor : public base, public util::message::error_token {
public:
  unbound_constructor(std::string_view value)
      : util::message::error_token(value) {}
  void describe(std::ostream &os) const {
    os << "unbound constructor  " << util::message::style::bold << token << util::message::style::clear;
  }
};
/*
class constructor_shouldnt_take_arg : public std::runtime_error, public util::error::report_token_error {
public:
  constructor_shouldnt_take_arg(std::string_view value)
      : std::runtime_error("name-resolving error"),
        util::error::report_token_error("Error: Constructor ", value, "; should not have an arg") {}
};

class constructor_should_take_arg : public std::runtime_error, public util::error::report_token_error {
public:
  constructor_should_take_arg(std::string_view value)
      : std::runtime_error("name-resolving error"),
        util::error::report_token_error("Error: Constructor ", value, "; should have an arg") {}
};
*/
}

struct locable {
  std::string_view loc;
  locable() = default;
  locable(std::string_view s) : loc(s) {}
  virtual ~locable() = default;
};

namespace type {

namespace expression {

struct t : public locable, public texp_of_t {
  virtual ::type::expression::t to_type(const std::unordered_map<std::string_view, size_t> &vars_map,
                                        const ::type::type_map &type_map) const = 0;
};
typedef std::unique_ptr<t> ptr;

struct identifier : public t {
  typedef std::unique_ptr<identifier> ptr;
  std::string_view name;
  identifier(std::string_view s) : name(s) {}
  ::type::expression::t to_type(const std::unordered_map<std::string_view, size_t> &vars_map,
                                const ::type::type_map &type_map) const final;
TO_TEXP(name);
}; //e.g. 'a or int
struct function : public t {
  ptr from, to;
  function(ptr &&f, ptr &&x) : from(std::move(f)), to(std::move(x)) { loc = unite_sv(from, to); }
  ::type::expression::t to_type(const std::unordered_map<std::string_view, size_t> &vars_map,
                                const ::type::type_map &type_map) const final;
TO_TEXP(from, to);
};
struct product : public t {
  typedef std::unique_ptr<product> ptr;

  std::vector<expression::ptr> ts; //size>=2
  //void set_loc() {loc = itr_sv(ts.front()->loc.begin(),ts.back()->loc.end());}
  ::type::expression::t to_type(const std::unordered_map<std::string_view, size_t> &vars_map,
                                const ::type::type_map &type_map) const final;

TO_TEXP(ts);
};
struct tuple : public t {
  typedef std::unique_ptr<tuple> ptr;

  std::vector<expression::ptr> ts; //size>=2
  tuple(expression::ptr &&x) { ts.push_back(std::move(x)); }
  //void set_loc() {loc = itr_sv(ts.front()->loc.begin(),ts.back()->loc.end());}
  ::type::expression::t to_type(const std::unordered_map<std::string_view, size_t> &vars_map,
                                const ::type::type_map &type_map) const final;

TO_TEXP(ts);
}; //not really a valid type-expression per se, but used in construction
struct application : public t {
  ptr x;
  identifier::ptr f;
  application(ptr &&xx, identifier::ptr &&ff) : x(std::move(xx)), f(std::move(ff)) {
    loc = itr_sv(x->loc.begin(),
                 f->loc.end());
  }
  ::type::expression::t to_type(const std::unordered_map<std::string_view, size_t> &vars_map,
                                const ::type::type_map &type_map) const final;

TO_TEXP(x, f)
};

}

namespace definition {

struct param : public locable, public texp_of_t {
  typedef std::unique_ptr<param> ptr;
  param(std::string_view s) : name(s) {}
  std::string_view name;
TO_TEXP(name);
};

struct single : public locable, public texp_of_t {
  typedef std::unique_ptr<single> ptr;
  std::vector<param::ptr> params;
  std::string_view name;
};

struct t : public locable, public texp_of_t {
  bool nonrec = false;
  std::vector<single::ptr> defs;
TO_TEXP(nonrec, defs);
};
typedef std::unique_ptr<t> ptr;

struct single_texpr : public single {
  typedef std::unique_ptr<single_texpr> ptr;

  expression::ptr type;
TO_TEXP(name, type);
};

struct single_variant : public single {
  typedef std::unique_ptr<single_variant> ptr;

  struct constr : public texp_of_t {
    bool is_immediate() const { THROW_INTERNAL_ERROR /*method removed*/ } //@deprecated
    std::string_view name;
    std::vector<expression::ptr> args;
  TO_TEXP(name, args);
  };

  std::vector<constr> variants;
TO_TEXP(name, variants);

};

}

}
typedef ::type::constr_map constr_map;
namespace expression {
struct t;
typedef std::unique_ptr<t> ptr;
struct identifier;
}

namespace matcher {
struct t;
typedef std::unique_ptr<t> ptr;
struct universal;
}

namespace literal {
struct t;
typedef std::unique_ptr<t> ptr;
}

namespace definition {
struct t;
typedef std::unique_ptr<t> ptr;
}

typedef std::unordered_map<std::string_view, matcher::universal *> global_names_map;
typedef std::unordered_map<const matcher::universal *, ::type::expression::t> global_types_map;
typedef std::unordered_map<const matcher::universal *, ::type::arena::idx_t> local_types_map;
struct tc_section {
  const global_types_map &global;
  local_types_map &local;
  ::type::arena &arena;
  typedef ::type::arena::idx_t idx_t;
};
struct tr_section {
  const global_types_map &global;
  local_types_map &local;
  ::type::arena &arena;
  bool verbose;
  std::ostream &os;
  typedef ::type::arena::idx_t idx_t;
};
typedef std::forward_list<expression::identifier *> usage_list;
typedef std::unordered_map<std::string_view, usage_list> free_vars_t;
typedef std::unordered_set<const matcher::universal *> capture_set;

namespace expression {

struct t : public locable, public texp_of_t {
  using locable::locable;
  virtual free_vars_t free_vars() = 0; // computes the free variable of an expression
  virtual capture_set capture_group() = 0; // computes the set of non-global universal_macthers free in e
  virtual ir::lang::var ir_compile(ir_sections_t) = 0; // generate ir code, returning the var containing the result
  virtual void bind(const ::type::constr_map &) = 0;
  virtual bool is_constexpr() const = 0;
  virtual tc_section::idx_t typecheck(tc_section tcs) const = 0;

};

struct literal : public t {
  typedef std::unique_ptr<literal> ptr;
  ast::literal::ptr value;
  literal(ast::literal::ptr &&v);
  free_vars_t free_vars() final;;
  capture_set capture_group() final;;
  ir::lang::var ir_compile(ir_sections_t) final;
  void bind(const constr_map &) final {}
  tc_section::idx_t typecheck(tc_section tcs) const final;
  bool is_constexpr() const final { return true; }
TO_TEXP(value);
};

struct identifier : public t {
  free_vars_t free_vars() final;
  const matcher::universal *definition_point;
  explicit identifier(std::string_view n);
  explicit identifier(std::string_view n, std::string_view loc);
  capture_set capture_group() final;
  std::string_view name;
  ir::lang::var ir_compile(ir_sections_t) final;
  void bind(const constr_map &) final;
  bool is_constexpr() const final { return true; }
TO_TEXP(name);
  tc_section::idx_t typecheck(tc_section tcs) const final;
};

struct constructor : public t {
  free_vars_t free_vars() final;;
  capture_set capture_group() final;;
  explicit constructor(std::string_view n);
  std::string_view name;
  ptr arg; // either null, an expression, a_tuple
  const ::type::function::variant::constr *definition_point;
  ir::lang::var ir_compile(ir_sections_t) final;
  ir::lang::var ir_compile_with_destructor(ir_sections_t, ir::lang::var d) const;
  void bind(const constr_map &cm) final;
  bool is_constexpr() const final { return arg ? arg->is_constexpr() : true; }
TO_TEXP(name, arg);
  tc_section::idx_t typecheck(tc_section tcs) const final;

};

struct if_then_else : public t {
  expression::ptr condition, true_branch, false_branch;
  if_then_else(expression::ptr &&condition, expression::ptr &&true_branch, expression::ptr &&false_branch);
  free_vars_t free_vars() final;
  capture_set capture_group() final;
  ir::lang::var ir_compile(ir_sections_t) final;
  void bind(const constr_map &cm) final;
  bool is_constexpr() const final {
    return condition->is_constexpr() && true_branch->is_constexpr() && false_branch->is_constexpr();
  }
TO_TEXP(condition, true_branch, false_branch);
  tc_section::idx_t typecheck(tc_section tcs) const final;

};

struct tuple : public t {
  std::vector<expression::ptr> args;
  explicit tuple(std::vector<expression::ptr> &&args);
  tuple() = default;
  free_vars_t free_vars() final;
  capture_set capture_group() final;;
  ir::lang::var ir_compile(ir_sections_t) final;
  ir::lang::var ir_compile_with_destructor(ir_sections_t, ir::lang::var d);
  void bind(const constr_map &cm) final;
  bool is_constexpr() const final {
    return std::all_of(args.begin(),
                       args.end(),
                       [](const auto &p) { return p->is_constexpr(); });
  }

TO_TEXP(args);
  tc_section::idx_t typecheck(tc_section tcs) const final;

};

struct fun_app : public t {
  expression::ptr f, x;
  fun_app(expression::ptr &&f_, expression::ptr &&x_);
  free_vars_t free_vars() final;
  capture_set capture_group() final;
  ir::lang::var ir_compile(ir_sections_t) final;
  void bind(const constr_map &cm) final;
  bool is_constexpr() const final { return false; }

TO_TEXP(f, x);
  tc_section::idx_t typecheck(tc_section tcs) const final;

};

struct destroy : public t {
  expression::ptr obj, d;
  destroy(expression::ptr &&obj, expression::ptr &&d);
  free_vars_t free_vars() final;
  capture_set capture_group() final;
  ir::lang::var ir_compile(ir_sections_t) final;
  void bind(const constr_map &cm) final;
  bool is_constexpr() const final { return false; }

TO_TEXP(obj, d);
  tc_section::idx_t typecheck(tc_section tcs) const final;

};

struct seq : public t {
  expression::ptr a, b;
  seq(expression::ptr &&a, expression::ptr &&b);
  free_vars_t free_vars() final;;
  capture_set capture_group() final;
  ir::lang::var ir_compile(ir_sections_t) final;
  void bind(const constr_map &cm) final;
  bool is_constexpr() const final { return a->is_constexpr() && b->is_constexpr(); }

TO_TEXP(a, b);
  tc_section::idx_t typecheck(tc_section tcs) const final;

};

struct match_with : public t {
  typedef std::unique_ptr<match_with> ptr;
  struct branch : public texp_of_t {
    matcher::ptr pattern;
    expression::ptr result;
  TO_TEXP(pattern, result)
  };
  expression::ptr what;
  std::vector<branch> branches;
  explicit match_with(expression::ptr &&w);
  free_vars_t free_vars() final;;

  capture_set capture_group() final;
  ir::lang::var ir_compile(ir_sections_t) final;
  void bind(const constr_map &cm) final;
  bool is_constexpr() const final { return false; }

TO_TEXP(what, branches);
  tc_section::idx_t typecheck(tc_section tcs) const final;

};

struct let_in : public t {
  definition::ptr d;
  expression::ptr e;
  let_in(definition::ptr &&d, expression::ptr &&e) : d(std::move(d)), e(std::move(e)) {}
  free_vars_t free_vars() final;
  capture_set capture_group() final;
  ir::lang::var ir_compile(ir_sections_t) final;
  void bind(const constr_map &cm) final;
  bool is_constexpr() const final { return false; }
TO_TEXP(d, e);
  tc_section::idx_t typecheck(tc_section tcs) const final;

};

struct fun : public t {
  std::vector<matcher::ptr> args;
  expression::ptr body;
  std::vector<const matcher::universal *> captures;
  fun(std::vector<matcher::ptr> &&args, expression::ptr &&body);
  free_vars_t free_vars() final;;
  capture_set capture_group() final;;
  static std::string text_name_gen(std::string_view name_hint);
  ir::lang::var ir_compile(ir_sections_t) final;
  void bind(const constr_map &cm) final;
  bool is_capturing(const matcher::universal *m) const;
  size_t capture_index(const matcher::universal *m) const;
  bool is_constexpr() const final { return true; }
TO_TEXP(args, body);
  std::string ir_compile_global(ir_sections_t s);
  tc_section::idx_t typecheck(tc_section tcs) const final;

};

}

namespace matcher {
struct t : public locable, texp_of_t {
  virtual std::ostream &print(std::ostream &) const = 0;
  virtual void bind(free_vars_t &) = 0;
  virtual void bind(capture_set &) = 0;
  virtual void bind(const constr_map &) = 0;
  virtual void ir_globally_register(global_names_map &) = 0;
  virtual void ir_allocate_global_value(std::ostream &os) = 0;
  virtual void ir_global_unroll(ir::scope &s, ir::lang::var v) = 0; // match value in v, unrolling on globals
  virtual void ir_locally_unroll(ir::scope &s, ir::lang::var v) = 0; // match value in v, unrolling on locals
  virtual ir::lang::var ir_test_unroll(ir::scope &s, ir::lang::var v) = 0;
  virtual tc_section::idx_t typecheck(tc_section tcs) const = 0;
  virtual std::list<const universal *> universals() const = 0;
  virtual void for_each_universal(const std::function<void(universal&)>& f) = 0;
};
struct universal : public t {
  typedef std::unique_ptr<universal> ptr;
  size_t stack_relative_pos = -1;
  bool use_as_immediate = false;
  size_t name_resolution_id = 0;
  bool top_level =
      false; // True only for toplevel definition - this means
  // 1. It has a static address which doesn't expire
  // 2. Hence closures do not need to capture this
  // 3. It's type doesn't get changed by type inference - unified on a "per use" basis
  ir::lang::var ir_var; //useless if toplevel; otherwise identifies which var to use (both locally and captured)
  explicit universal(std::string_view n);
  void bind(free_vars_t &fv) final;
  void bind(capture_set &cs) final;
  void bind(const constr_map &cm) final;

  std::string asm_name() const;
  std::string ir_asm_name() const;

  std::ostream &print(std::ostream &os) const final { return os << name; }

  void ir_globally_register(global_names_map &m) final;
  void ir_allocate_global_value(std::ostream &os) final;
  void ir_allocate_global_tuple(std::ostream &os, size_t tuple_size);

  void ir_allocate_global_constrblock(std::ostream &os, const ::type::function::variant::constr &constr);
  void ir_allocate_global_constrimm(std::ostream &os, const ::type::function::variant::constr &constr);
  void ir_global_unroll(ir::scope &s, ir::lang::var) final;
  void ir_locally_unroll(ir::scope &s, ir::lang::var v) final;
  ir::lang::var ir_test_unroll(ir::scope &s, ir::lang::var v) final { return s.declare_constant(3); }
  std::string_view name;
  usage_list usages;
TO_TEXP(name)
  void ir_allocate_globally_funblock(std::ostream &os, size_t n_args, std::string_view text_ptr);
  ir::var ir_evaluate_global(ir::scope &s) const;
  tc_section::idx_t typecheck(tc_section tcs) const final;
  std::list<const universal *> universals() const final;
  void for_each_universal(const std::function<void(universal&)>& f) final {
    return f(*this);
  }
};

struct ignore : public t {
  typedef std::unique_ptr<ignore> ptr;
  std::ostream &print(std::ostream &os) const final { return os << "_"; }
  void bind(free_vars_t &fv) final;
  void bind(capture_set &cs) final;
  void bind(const constr_map &cm) final;
  void ir_allocate_global_value(std::ostream &os) final;
  void ir_globally_register(global_names_map &m) final;
  void ir_global_unroll(ir::scope &s, ir::lang::var) final {}
  void ir_locally_unroll(ir::scope &s, ir::lang::var v) final {}
  ir::lang::var ir_test_unroll(ir::scope &s, ir::lang::var v) final { return s.declare_constant(3); }
  tc_section::idx_t typecheck(tc_section tcs) const final;
  std::list<const universal *> universals() const final;
  void for_each_universal(const std::function<void(universal&)>& f) final {}
TO_TEXP_EMPTY()
};

struct constructor : public t {
  typedef std::unique_ptr<constructor> ptr;
  matcher::ptr arg;
  std::string_view cons;
  const ::type::function::variant::constr *definition_point;
  constructor(matcher::ptr &&m, std::string_view c) : arg(std::move(m)), cons(c) {}
  constructor(std::string_view c) : arg(), cons(c) {}
  std::ostream &print(std::ostream &os) const final {
    return arg ? arg->print(os << definition_point->name << " ") : (os << definition_point->name);
  }
  void bind(free_vars_t &fv) final;
  void bind(capture_set &cs) final;
  void ir_globally_register(global_names_map &m) final;
  void bind(const constr_map &cm) final;
  void ir_allocate_global_value(std::ostream &os) final;
  void ir_global_unroll(ir::scope &s, ir::lang::var) final;
  void ir_locally_unroll(ir::scope &s, ir::lang::var v) final;
  ir::lang::var ir_test_unroll(ir::scope &s, ir::lang::var v) final;
  tc_section::idx_t typecheck(tc_section tcs) const final;
  std::list<const universal *> universals() const final;
  void for_each_universal(const std::function<void(universal&)>& f) final {if(arg)arg->for_each_universal(f);}
TO_TEXP(cons, arg);
};

struct literal : public t {
  typedef std::unique_ptr<literal> ptr;
  ast::literal::ptr value;
  std::ostream &print(std::ostream &os) const final {
    assert(!loc.empty());
    return os << loc;
  }
  literal(ast::literal::ptr &&lit) : value(std::move(lit)) {}
  void bind(free_vars_t &fv) final {}
  void bind(capture_set &cs) final {}
  void bind(const constr_map &cm) final {}
  void ir_globally_register(global_names_map &m) final {}
  void ir_allocate_global_value(std::ostream &os) final {}
  void ir_global_unroll(ir::scope &s, ir::lang::var) final {}
  void ir_locally_unroll(ir::scope &s, ir::lang::var v) final {}
  ir::lang::var ir_test_unroll(ir::scope &s, ir::lang::var v) final;
  tc_section::idx_t typecheck(tc_section tcs) const final;
  std::list<const universal *> universals() const final;
  void for_each_universal(const std::function<void(universal&)>& f) final {}
TO_TEXP(value);
};

struct tuple : public t {
  std::vector<matcher::ptr> args;
  tuple() = default;
  void bind(free_vars_t &fv) final;
  void bind(capture_set &cs) final;
  void bind(const constr_map &cm) final;
  std::ostream &print(std::ostream &os) const final;

  void ir_allocate_global_value(std::ostream &os) final;
  void ir_globally_register(global_names_map &m) final;
  void ir_global_unroll(ir::scope &s, ir::lang::var) final;
  void ir_locally_unroll(ir::scope &s, ir::lang::var v) final;
  ir::lang::var ir_test_unroll(ir::scope &main, ir::lang::var v) final;
  tc_section::idx_t typecheck(tc_section tcs) const final;
  std::list<const universal *> universals() const final;
  void for_each_universal(const std::function<void(universal&)>& f) final {for(auto &p : args)p->for_each_universal(f);}
TO_TEXP(args);
};

}

namespace literal {
struct t : public texp_of_t {
  virtual ~t() = default;
  virtual uint64_t to_value() const = 0;
  virtual ir::lang::var ir_compile(ir_sections_t) const = 0;
  virtual tc_section::idx_t typecheck(tc_section tcs) const = 0;
};
struct integer : public t {
  int64_t value;
  integer(int64_t value) : value(value) {}
  uint64_t to_value() const final;
  ir::lang::var ir_compile(ir_sections_t) const final;
  tc_section::idx_t typecheck(tc_section tcs) const final;
TO_TEXP(value)
};
struct boolean : public t {
  bool value;
  boolean(bool value) : value(value) {}
  uint64_t to_value() const final;
  ir::lang::var ir_compile(ir_sections_t) const final;
  tc_section::idx_t typecheck(tc_section tcs) const final;
TO_TEXP(value)
};
struct unit : public t {
  unit() {}
  uint64_t to_value() const final { return 1; }
  ir::lang::var ir_compile(ir_sections_t) const final;
  tc_section::idx_t typecheck(tc_section tcs) const final;

TO_TEXP_EMPTY()
};
struct string : public t {
  std::string value;
  string(std::string_view value) : value(value) {}
  string(std::string &&value) : value(std::move(value)) {}
  uint64_t to_value() const final;
  ir::lang::var ir_compile(ir_sections_t) const final;
  tc_section::idx_t typecheck(tc_section tcs) const final;

TO_TEXP(value)
};
}

namespace definition {

struct def : public texp_of_t {
  matcher::ptr name;
  expression::ptr e;
  def() = default;
  def(matcher::ptr &&name, expression::ptr e);
  bool is_fun() const;
  bool is_constr() const;
  bool is_tuple() const;
  bool is_single_name() const;
  void typecheck(tc_section tcs) const;

TO_TEXP(name, e);
};
struct t : public locable, public texp_of_t {
  bool rec = false;

  std::vector<def> defs;
  free_vars_t free_vars(free_vars_t &&fv = {});
  capture_set capture_group(capture_set &&cs = {});

  void ir_compile_global(ir_sections_t s);
  void ir_compile_locally(ir_sections_t s);
  void typecheck(tc_section tcs) const;

  void bind(const constr_map &cm);

TO_TEXP(rec, defs);
};
typedef std::unique_ptr<t> ptr;

}

}

#endif //COMPILERS_BML_LIB_AST_H_
