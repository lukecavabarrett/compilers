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

namespace ast {
using namespace util;
typedef texp::texp_of_t texp_of_t;

namespace {
template<typename P1, typename P2>
std::string_view unite_sv(const P1 &p1, const P2 &p2) {
  return itr_sv(p1->loc.begin(), p2->loc.end());
}

}

namespace {

class unbound_value : public std::runtime_error, public util::error::report_token_error {
 public:
  unbound_value(std::string_view value) : std::runtime_error("name-resolving error"), util::error::report_token_error("Error: Unbound value ", value, "; maybe you forget a rec or you mispelt something.") {}
};

class unbound_constructor : public std::runtime_error, public util::error::report_token_error {
 public:
  unbound_constructor(std::string_view value) : std::runtime_error("name-resolving error"), util::error::report_token_error("Error: Unbound constructor ", value, "; maybe you mispelt something.") {}
};

class constructor_shouldnt_take_arg : public std::runtime_error, public util::error::report_token_error {
 public:
  constructor_shouldnt_take_arg(std::string_view value) : std::runtime_error("name-resolving error"), util::error::report_token_error("Error: Constructor ", value, "; should not have an arg") {}
};

class constructor_should_take_arg : public std::runtime_error, public util::error::report_token_error {
 public:
  constructor_should_take_arg(std::string_view value) : std::runtime_error("name-resolving error"), util::error::report_token_error("Error: Constructor ", value, "; should have an arg") {}
};

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
};
typedef std::unique_ptr<t> ptr;

struct identifier : public t {
  typedef std::unique_ptr<identifier> ptr;
  std::string_view name;
  identifier(std::string_view s) : name(s) {}
 TO_TEXP(name);
}; //e.g. 'a or int
struct function : public t {
  ptr from, to;
  function(ptr &&f, ptr &&x) : from(std::move(f)), to(std::move(x)) { loc = unite_sv(from, to); }
 TO_TEXP(from, to);
};
struct product : public t {
  typedef std::unique_ptr<product> ptr;

  std::vector<expression::ptr> ts; //size>=2
  //void set_loc() {loc = itr_sv(ts.front()->loc.begin(),ts.back()->loc.end());}
 TO_TEXP(ts);
};
struct tuple : public t {
  typedef std::unique_ptr<tuple> ptr;

  std::vector<expression::ptr> ts; //size>=2
  tuple(expression::ptr &&x) { ts.push_back(std::move(x)); }
  //void set_loc() {loc = itr_sv(ts.front()->loc.begin(),ts.back()->loc.end());}
 TO_TEXP(ts);
};
struct constr : public t {
  ptr x, f;
  constr(ptr &&xx, ptr &&ff) : x(std::move(xx)), f(std::move(ff)) { loc = itr_sv(x->loc.begin(), f->loc.end()); }
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
    bool is_immediate() const { return type == nullptr; }
    uint64_t tag;
    std::string_view name;
    expression::ptr type;
   TO_TEXP(name, type);
  };

  std::vector<constr> variants;
 TO_TEXP(name, variants);

};

}

}

namespace expression {
struct t;
typedef std::unique_ptr<t> ptr;
struct identifier;
}

namespace matcher {
struct t;
typedef std::unique_ptr<t> ptr;
struct universal_matcher;
}

namespace literal {
struct t;
typedef std::unique_ptr<t> ptr;
}

namespace definition {
struct t;
typedef std::unique_ptr<t> ptr;
}

typedef std::unordered_map<std::string_view, ast::type::definition::single_variant::constr *> constr_map;
typedef std::unordered_map<std::string_view, matcher::universal_matcher *> global_map;
typedef std::forward_list<expression::identifier *> usage_list;
typedef std::unordered_map<std::string_view, usage_list> free_vars_t;
typedef std::unordered_set<const matcher::universal_matcher *> capture_set;

namespace expression {

struct t : public locable, public texp_of_t {
  using locable::locable;
  virtual free_vars_t free_vars() = 0; // computes the free variable of an expression
  virtual capture_set capture_group() = 0; // computes the set of non-global universal_macthers free in e
  virtual void compile(direct_sections_t s, size_t stack_pos) = 0; // generate code putting the result on rax
  virtual ir::lang::var ir_compile(ir_sections_t) = 0; // generate ir code, returning the var containing the result
  virtual void bind(const constr_map &) = 0;
};

struct literal : public t {
  typedef std::unique_ptr<literal> ptr;
  ast::literal::ptr value;
  literal(ast::literal::ptr &&v);
  free_vars_t free_vars() final;;
  capture_set capture_group() final;;
  void compile(direct_sections_t s, size_t stack_pos) final;
  ir::lang::var ir_compile(ir_sections_t) final;
  void bind(const constr_map &) final {}
 TO_TEXP(value);
};

struct identifier : public t {
  free_vars_t free_vars() final;
  const matcher::universal_matcher *definition_point;
  explicit identifier(std::string_view n);
  capture_set capture_group() final;
  std::string_view name;
  void compile(direct_sections_t s, size_t stack_pos) final;
  ir::lang::var ir_compile(ir_sections_t) final;
  void bind(const constr_map &) final;
 TO_TEXP(name);
};

struct constructor : public t {
  free_vars_t free_vars() final;;
  capture_set capture_group() final;;
  explicit constructor(std::string_view n);
  std::string_view name;
  ptr arg;
  type::definition::single_variant::constr *definition_point;
  void compile(direct_sections_t s, size_t stack_pos) final;
  ir::lang::var ir_compile(ir_sections_t) final;
  void bind(const constr_map &cm) final;
 TO_TEXP(name, arg);
};

struct if_then_else : public t {
  expression::ptr condition, true_branch, false_branch;
  if_then_else(expression::ptr &&condition, expression::ptr &&true_branch, expression::ptr &&false_branch);
  free_vars_t free_vars() final;
  capture_set capture_group() final;
  void compile(direct_sections_t s, size_t stack_pos) final;
  ir::lang::var ir_compile(ir_sections_t) final;
  void bind(const constr_map &cm) final;
 TO_TEXP(condition, true_branch, false_branch);
};

struct build_tuple : public t {
  std::vector<expression::ptr> args;
  explicit build_tuple(std::vector<expression::ptr> &&args);
  build_tuple() = default;
  free_vars_t free_vars() final;
  capture_set capture_group() final;;
  void compile(direct_sections_t s, size_t stack_pos) final;
  ir::lang::var ir_compile(ir_sections_t) final;
  void bind(const constr_map &cm) final;
 TO_TEXP(args);
};

struct fun_app : public t {
  expression::ptr f, x;
  fun_app(expression::ptr &&f_, expression::ptr &&x_);
  free_vars_t free_vars() final;
  capture_set capture_group() final;
  void compile(direct_sections_t s, size_t stack_pos) final;
  ir::lang::var ir_compile(ir_sections_t) final;
  void bind(const constr_map &cm) final;
 TO_TEXP(f, x);
};

struct seq : public t {
  expression::ptr a, b;
  seq(expression::ptr &&a, expression::ptr &&b);
  free_vars_t free_vars() final;;
  capture_set capture_group() final;
  void compile(direct_sections_t s, size_t stack_pos) final;
  ir::lang::var ir_compile(ir_sections_t) final;
  void bind(const constr_map &cm) final;
 TO_TEXP(a, b);
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
  void compile(direct_sections_t s, size_t stack_pos) final;
  ir::lang::var ir_compile(ir_sections_t) final;
  void bind(const constr_map &cm) final;
 TO_TEXP(what, branches);
};

struct let_in : public t {
  definition::ptr d;
  expression::ptr e;
  let_in(definition::ptr &&d, expression::ptr &&e) : d(std::move(d)), e(std::move(e)) {}
  free_vars_t free_vars() final;;
  capture_set capture_group() final;;
  void compile(direct_sections_t s, size_t stack_pos) final;
  ir::lang::var ir_compile(ir_sections_t) final;
  void bind(const constr_map &cm) final;
 TO_TEXP(d, e);
};

struct fun : public t {
  std::vector<matcher::ptr> args;
  expression::ptr body;
  std::vector<const matcher::universal_matcher *> captures;
  fun(std::vector<matcher::ptr> &&args, expression::ptr &&body);
  free_vars_t free_vars() final;;
  capture_set capture_group() final;;
  static std::string text_name_gen(std::string_view name_hint);
  std::string compile_global(direct_sections_t s, std::string_view name_hint = ""); // compile the body of the function, and return the text_ptr
  void compile(direct_sections_t s, size_t stack_pos) final;
  ir::lang::var ir_compile(ir_sections_t) final;
  void bind(const constr_map &cm) final;
  bool is_capturing(const matcher::universal_matcher *m) const;
  size_t capture_index(const matcher::universal_matcher *m) const;
 TO_TEXP(args, body);
  std::string ir_compile_global(ir_sections_t s);
};

}

namespace matcher {
struct t : public locable, texp_of_t {
  virtual void bind(free_vars_t &) = 0;
  virtual void bind(capture_set &) = 0;
  virtual void bind(const constr_map &) = 0;
  virtual void globally_register(global_map &) = 0;
  virtual void ir_globally_register(global_map &) = 0;
  virtual void globally_allocate(std::ostream &os) = 0;
  virtual void ir_allocate_global_value(std::ostream &os) = 0;
  virtual size_t unrolled_size() const = 0; // number of universal_matchers contained
  virtual size_t stack_unrolling_dimension() const = 0; // how much stack is going to be used for unrolling
  virtual void global_unroll(std::ostream &os) = 0; // match value in rax, unrolling on globals
  virtual void ir_global_unroll(ir::scope &s, ir::lang::var v) = 0; // match value in v, unrolling on globals
  virtual void ir_locally_unroll(ir::scope& s, ir::lang::var v) = 0; // match value in v, unrolling on locals
  virtual ir::lang::var ir_test_unroll(ir::scope& s, ir::lang::var v) = 0;
  virtual size_t locally_unroll(std::ostream &os, size_t stack_pos) = 0;  // match value in rax, unrolling on stack; returns new stack_pos
  virtual size_t test_locally_unroll(std::ostream &os,
                                     size_t stack_pos,
                                     size_t caller_stack_pos,
                                     std::string_view on_fail) = 0; // match value in rax, on_success unrolling on stack (WITHOUT moving rsp - it'll be incremented by the user); returns new stack_pos; on failure clean stack and jmp to on_fail
};
struct universal_matcher : public t {
  typedef std::unique_ptr<universal_matcher> ptr;
  size_t stack_relative_pos = -1;
  bool use_as_immediate = false;
  size_t name_resolution_id = 0;
  bool top_level =
      false; // True only for toplevel definition - this means
  // 1. It has a static address which doesn't expire
  // 2. Hence closures do not need to capture this
  // 3. It's type doesn't get changed by type inference - unified on a "per use" basis
  ir::lang::var ir_var; //useless if toplevel; otherwise identifies which var to use (both locally and captured)
  explicit universal_matcher(std::string_view n);
  void bind(free_vars_t &fv) final;
  void bind(capture_set &cs) final;
  void bind(const constr_map &cm) final;

  std::string asm_name() const;
  std::string ir_asm_name() const;


  void globally_register(global_map &m) final;
  void ir_globally_register(global_map &m) final;
  void globally_allocate(std::ostream &os) final;
  void ir_allocate_global_value(std::ostream &os) final;
  void globally_allocate_funblock(size_t n_args, std::ostream &os, std::string_view text_ptr);
  void globally_allocate_tupleblock(std::ostream &os, size_t tuple_size);
  void ir_allocate_global_tuple(std::ostream &os, size_t tuple_size);

  void ir_allocate_global_constrblock(std::ostream &os, const type::definition::single_variant::constr &constr);
  void globally_allocate_constrblock(std::ostream &os, const type::definition::single_variant::constr &constr);
  void globally_allocate_constrimm(std::ostream &os, const type::definition::single_variant::constr &constr);
  void ir_allocate_global_constrimm(std::ostream &os, const type::definition::single_variant::constr &constr);
  void global_unroll(std::ostream &os) final;
  void ir_global_unroll(ir::scope &s, ir::lang::var ) final;
  void ir_locally_unroll(ir::scope& s, ir::lang::var v) final;
  ir::lang::var ir_test_unroll(ir::scope& s, ir::lang::var v) final {return s.declare_constant(3);}
  size_t unrolled_size() const final { return 1; }
  size_t stack_unrolling_dimension() const final { return 1; }
  size_t locally_unroll(std::ostream &os, size_t stack_pos) final;
  size_t test_locally_unroll(std::ostream &os, size_t stack_pos, size_t caller_stack_pos, std::string_view on_fail) final;
  void globally_evaluate(std::ostream &os) const;
  std::string_view name;
  usage_list usages;
 TO_TEXP(name)
  void ir_allocate_globally_funblock(std::ostream &os, size_t n_args, std::string_view text_ptr);
  ir::var ir_evaluate_global(ir::scope &s) const;
};

struct anonymous_universal_matcher : public t {
  typedef std::unique_ptr<anonymous_universal_matcher> ptr;
  void bind(free_vars_t &fv) final {}
  void bind(capture_set &cs) final {}
  void bind(const constr_map &cm) final {}
  void ir_allocate_global_value(std::ostream &os) final {}
  void globally_allocate(std::ostream &os) final {}
  void globally_register(global_map &m) final {}
  void ir_globally_register(global_map &m) final {}
  void global_unroll(std::ostream &os) final {}
  void ir_global_unroll(ir::scope &s, ir::lang::var ) final {}
  void ir_locally_unroll(ir::scope& s, ir::lang::var v) final {}
  size_t unrolled_size() const final { return 0; }
  size_t stack_unrolling_dimension() const final { return 0; }
  size_t locally_unroll(std::ostream &os, size_t stack_pos) final { return stack_pos; }
  size_t test_locally_unroll(std::ostream &os, size_t stack_pos, size_t caller_stack_pos, std::string_view on_fail) final { return stack_pos; }
  ir::lang::var ir_test_unroll(ir::scope& s, ir::lang::var v) final {return s.declare_constant(3);}
 TO_TEXP_EMPTY()
};

struct constructor_matcher : public t {
  typedef std::unique_ptr<constructor_matcher> ptr;
  matcher::ptr arg;
  std::string_view cons;
  type::definition::single_variant::constr *definition_point;
  constructor_matcher(matcher::ptr &&m, std::string_view c) : arg(std::move(m)), cons(c) {}
  constructor_matcher(std::string_view c) : arg(), cons(c) {}
  void bind(free_vars_t &fv) final;
  void bind(capture_set &cs) final;
  void globally_register(global_map &m) final;
  void ir_globally_register(global_map &m) final;
  void bind(const constr_map &cm) final;
  void ir_allocate_global_value(std::ostream &os) final {if(arg)arg->ir_allocate_global_value(os);}
  void globally_allocate(std::ostream &os) final { if (arg)arg->globally_allocate(os); }
  void global_unroll(std::ostream &os) final;
  void ir_global_unroll(ir::scope &s, ir::lang::var) final;
  void ir_locally_unroll(ir::scope& s, ir::lang::var v) final;
  size_t unrolled_size() const final { return arg ? arg->unrolled_size() : 0; }
  size_t stack_unrolling_dimension() const final { return arg ? arg->stack_unrolling_dimension() : 0; }
  size_t locally_unroll(std::ostream &os, size_t stack_pos) final;
  size_t test_locally_unroll(std::ostream &os, size_t stack_pos, size_t caller_stack_pos, std::string_view on_fail) final;
  ir::lang::var ir_test_unroll(ir::scope& s, ir::lang::var v) final;
 TO_TEXP(cons, arg);
};

struct literal_matcher : public t {
  typedef std::unique_ptr<literal_matcher> ptr;
  ast::literal::ptr value;
  literal_matcher(ast::literal::ptr &&lit) : value(std::move(lit)) {}
  void bind(free_vars_t &fv) final {}
  void bind(capture_set &cs) final {}
  void bind(const constr_map &cm) final {}
  void globally_allocate(std::ostream &os) final {}
  void globally_register(global_map &m) final {}
  void ir_globally_register(global_map &m) final {}
  void ir_allocate_global_value(std::ostream &os) final {}
  void global_unroll(std::ostream &os) final {}
  void ir_global_unroll(ir::scope &s, ir::lang::var) final {}
  void ir_locally_unroll(ir::scope& s, ir::lang::var v) final {}
  size_t unrolled_size() const final { return 0; }
  size_t stack_unrolling_dimension() const final { return 0; }
  size_t locally_unroll(std::ostream &os, size_t stack_pos) final { return stack_pos; }
  size_t test_locally_unroll(std::ostream &os, size_t stack_pos, size_t caller_stack_pos, std::string_view on_fail) final;
  ir::lang::var ir_test_unroll(ir::scope& s, ir::lang::var v) final  {THROW_UNIMPLEMENTED;}
 TO_TEXP(value);
};

struct tuple_matcher : public t {
  std::vector<matcher::ptr> args;
  tuple_matcher() = default;
  void bind(free_vars_t &fv) final;
  void bind(capture_set &cs) final;
  void bind(const constr_map &cm) final;

  void globally_allocate(std::ostream &os) final;
  void ir_allocate_global_value(std::ostream &os) final;
  size_t unrolled_size() const final;
  size_t stack_unrolling_dimension() const final;
  void globally_register(global_map &m) final;
  void ir_globally_register(global_map &m) final;
  void global_unroll(std::ostream &os) final;
  void ir_global_unroll(ir::scope &s, ir::lang::var) final;
  void ir_locally_unroll(ir::scope& s, ir::lang::var v) final;
  size_t locally_unroll(std::ostream &os, size_t stack_pos) final;
  size_t test_locally_unroll(std::ostream &os, size_t stack_pos, size_t caller_stack_pos, std::string_view on_fail) final;
  ir::lang::var ir_test_unroll(ir::scope& s, ir::lang::var v) final {THROW_UNIMPLEMENTED;}
 TO_TEXP(args);
};

}

namespace literal {
struct t : public texp_of_t {
  virtual ~t() = default;
  virtual uint64_t to_value() const = 0;
};
struct integer : public t {
  int64_t value;
  integer(int64_t value) : value(value) {}
  uint64_t to_value() const final;
 TO_TEXP(value)
};
struct boolean : public t {
  bool value;
  boolean(bool value) : value(value) {}
  uint64_t to_value() const final;
 TO_TEXP(value)
};
struct unit : public t {
  unit() {}
  uint64_t to_value() const final { return 1; }
 TO_TEXP_EMPTY()
};
struct string : public t {
  std::string value;
  string(std::string_view value) : value(value) {}
  uint64_t to_value() const final;
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
 TO_TEXP(name, e);
};
struct t : public locable, public texp_of_t {
  bool rec = false;

  std::vector<def> defs;
  free_vars_t free_vars(free_vars_t &&fv = {});
  capture_set capture_group(capture_set &&cs = {});

  void ir_compile_global(ir_sections_t s);
  void ir_compile_locally(ir_sections_t s);
  void compile_global(direct_sections_t s);
  size_t compile_locally(direct_sections_t s, size_t stack_pos);

  void bind(const constr_map &cm);

 TO_TEXP(rec, defs);
};
typedef std::unique_ptr<t> ptr;

}

}

#endif //COMPILERS_BML_LIB_AST_H_
