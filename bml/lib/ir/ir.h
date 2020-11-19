#ifndef COMPILERS_BML_LIB_IR_IR_H_
#define COMPILERS_BML_LIB_IR_IR_H_

#include <lang.h>
#include <list>

namespace ir {

enum register_t {
  rbx = 0, rbp = 1, r12 = 2, r13 = 3, r14 = 4, r15 = 5, // non-volatile (callee-saved)
  rsi = 6, rdi = 7, rax = 8, rcx = 9, rdx = 10, r8 = 11, r9 = 12, r10 = 13, r11 = 14, // volatiles (caller-saved)
  // rsp is not managed this way// non-volatile (callee-saved)
};
namespace reg {
bool is_volatile(register_t r);
bool is_non_volatile(register_t r);
std::string_view to_string(register_t r);
constexpr auto all = util::make_array(rax, rcx, rdx, rsi, r8, r9, r10, r11, rbx, rbp, rdi, r12, r13, r14, r15);
constexpr auto non_volatiles = util::make_array(rbx, rbp, r12, r13, r14, r15);
constexpr auto volatiles = util::make_array(rax, rcx, rdx, rdi, rsi, r8, r9, r10, r11);
};
/*
namespace var_loc {
struct unborn { bool operator==(const unborn &) const { return true; }};
struct dead { bool operator==(const dead &) const { return true; }};
struct constant { uint64_t v; bool operator==(const constant &c) const { return c.v == v; }};
struct global { std::string name; bool operator==(const global &g) const { return g.name == name; }};
struct on_stack { size_t p; bool operator==(const on_stack &s) const { return s.p == p; }};
struct on_reg { register_t r; bool operator==(const on_reg &reg) const { return reg.r == r; }};
typedef std::variant<unborn, dead, constant, global, on_stack, on_reg> variant;
typedef std::variant<on_stack, on_reg> act_variant;
bool is_dead(const variant &v) {
  return std::holds_alternative<dead>(v);
}
bool is_stored(const variant &v) {
  return std::holds_alternative<on_stack>(v) || std::holds_alternative<on_reg>(v);
}
void print(const variant &v, std::ostream &os) {
  std::visit(overloaded{
      [&](const unborn &) { os << "unborn"; },
      [&](const dead &) { os << "dead"; },
      [&](const constant &c) { os << "constant " << c.v; },
      [&](const global &g) { os << "global " << g.name; },
      [&](const on_stack &s) { os << "on_stack[" << s.p << "]"; },
      [&](const on_reg &r) { os << "on_reg " << reg::to_string(r.r); },
  }, v);
}
void print(const act_variant &v, std::ostream &os) {
  std::visit(overloaded{
      [&](const on_stack &s) { os << "on_stack[" << s.p << "]"; },
      [&](const on_reg &r) { os << "on_reg " << reg::to_string(r.r); },
  }, v);
}
};
typedef var_loc::variant location_t;
typedef var_loc::act_variant act_location_t;
namespace content_opts {
struct free { bool operator==(const free &) const { return true; }};
struct saved_reg { register_t r; bool operator==(const saved_reg &sr) const { return r == sr.r; }};
struct store_var { var v; bool operator==(const store_var &sv) const { return v == sv.v; }};
typedef std::variant<free, saved_reg, store_var> variant;
bool is_free(const variant &v) {
  return std::holds_alternative<free>(v);
}
void print(const variant &v, std::ostream &os) {
  std::visit(overloaded{
      [&](const free &) { os << "free"; },
      [&](const saved_reg &r) { os << "saved " << reg::to_string(r.r); },
      [&](const store_var &v) { v.v.print(os); }
  }, v);
}
};
typedef content_opts::variant content_t;
*/

template<typename T>
struct lru_list {
 private:
  std::list<T> list;
  std::unordered_map<T, typename std::list<T>::iterator> its;
 public:
  lru_list() = default;
  lru_list(lru_list &&) = default;
  lru_list(const lru_list &o) : list(o.list) {
    for (auto it = list.begin(); it != list.end(); ++it)its[*it] = it;
  };
  lru_list &operator=(const lru_list &o) {
    list = o.list;
    its.clear();
    for (auto it = list.begin(); it != list.end(); ++it)its[*it] = it;
  };
  lru_list &operator=(lru_list &&o) = default;
  void push_back(const T &v) {
    list.push_back(v);
    its[v] = --list.end();
  }
  void push_front(const T &v) {
    list.push_front(v);
    its[v] = list.begin();
  }
  bool contains(const T &v) {
    return its.contains(v);
  }
  void erase(const T &v) {
    list.erase(its.at(v));
    its.erase(v);
  }
  bool empty() const { return list.empty(); }
  const T &front() const { return list.front(); }
  template<typename Pred>
  const T &front_if(Pred p) const {
    auto it = std::find_if(list.begin(), list.end(), p);
    if (it == list.end())THROW_INTERNAL_ERROR;
    return *it;
  }
  const T &back() const { return list.back(); }
  void pop() {
    T v = list.front();
    list.pop_front();
    its.erase(v);
  }
  void bring_front(const T &v) {
    list.erase(its.at(v));
    list.push_front(v);
    its.at(v) = list.begin();
  }
  void bring_back(const T &v) {
    list.erase(its.at(v));
    list.push_back(v);
    its.at(v) = --list.end();
  }
};

struct reg_lru {
 private:
  std::list<register_t> list, volatile_list, non_volatile_list;
  typedef typename std::list<register_t>::iterator list_it;
  std::array<std::pair<list_it, list_it>, reg::all.size()> its; //general iterator, specific iterator
  void restore_its() {
    for (auto it = list.begin(); it != list.end(); ++it)its[*it].first = it;
    for (auto it = volatile_list.begin(); it != volatile_list.end(); ++it)its[*it].second = it;
    for (auto it = non_volatile_list.begin(); it != non_volatile_list.end(); ++it)its[*it].second = it;
  }
 public:
  reg_lru() : list(), volatile_list(reg::volatiles.begin(), reg::volatiles.end()), non_volatile_list(reg::non_volatiles.begin(), reg::non_volatiles.end()) {
    list.insert(list.end(), reg::volatiles.begin(), reg::volatiles.end());
    list.insert(list.end(), reg::non_volatiles.begin(), reg::non_volatiles.end());
    restore_its();
  };
  reg_lru(reg_lru &&) = default;
  reg_lru(const reg_lru &o) : list(o.list), volatile_list(o.volatile_list), non_volatile_list(o.non_volatile_list) {
    restore_its();
  };
  reg_lru &operator=(const reg_lru &o) {
    list = o.list;
    volatile_list = o.volatile_list;
    non_volatile_list = o.volatile_list;
    restore_its();
    return *this;
  };
  reg_lru &operator=(reg_lru &&o) = default;

  register_t front() const { return list.front(); }
  register_t front_volatile() const { return volatile_list.front(); }
  register_t front_non_volatile() const { return non_volatile_list.front(); }
  template<typename Pred>
  register_t front_if(Pred p) const {
    auto it = std::find_if(list.begin(), list.end(), p);
    if (it == list.end())THROW_INTERNAL_ERROR;
    return *it;
  }
  void bring_front(register_t r) {
    auto&[gl_it, lc_it] = its[r];
    list.erase(gl_it);
    list.push_front(r);
    gl_it = list.begin();
    auto &lc_list = reg::is_volatile(r) ? volatile_list : non_volatile_list;
    lc_list.erase(gl_it);
    lc_list.push_front(r);
    gl_it = lc_list.begin();

  }
  void bring_back(register_t r) {
    auto&[gl_it, lc_it] = its[r];
    list.erase(gl_it);
    list.push_back(r);
    gl_it = --list.end();
    auto &lc_list = reg::is_volatile(r) ? volatile_list : non_volatile_list;
    lc_list.erase(gl_it);
    lc_list.push_back(r);
    gl_it = --lc_list.end();
  }
};

using namespace lang;
struct context_t {
 private:
  void assert_consistency() const;
 public:

  context_t();

  void destroy(var v, std::ostream &);
  void destroy(const std::vector<var> &, std::ostream &);
  void declare_const(var v, uint64_t value);
  void declare_global(var v, std::string_view name);
  void declare_copy(var dst, var src, std::ostream &os);
  void declare_move(var dst, var src);
  void declare_free(var v, std::ostream &os);
  void declare_in(var v, register_t r);// the register must have been empty before, and now its content it's a variable
  void make_non_mem(var v, std::ostream &os);
  void make_non_both_mem(var v1, var v2, std::ostream &os);
  void make_both_non_mem(var v1, var v2, std::ostream &os);
  static context_t merge(context_t c1, std::ostream &os1, context_t c2, std::ostream &os2);
  void return_clean(std::initializer_list<std::pair<var, register_t>> args, std::ostream &os); //that var will go in rax
  //clean for call - volatiles free.
  void call_clean(std::initializer_list<std::pair<var, register_t>> args, std::ostream &); // those variable will go in the specified volatile registers.
  void compress_stack(std::ostream &);

  struct streamable {};
  bool contains(var v) const;
  size_t stack_size() const;
  streamable at(var v) const;
  std::string retrieve_to_string(var v) const;
  void retrieve(var v, std::ostream &os) const;
  bool are_volatiles_free(std::initializer_list<std::pair<var, register_t>> except = {}) const;
  bool are_nonvolatiles_restored() const;
  bool is_stack_empty() const;
 private:
  reg_lru lru;

  struct constant { uint64_t value; bool operator==(constant o) const { return value == o.value; }};
  typedef std::string global;
  typedef size_t on_stack;
  typedef register_t on_reg;
  typedef std::variant<constant, global, on_stack, on_reg> location_t;

  std::unordered_map<var, std::variant<constant, global, on_stack, on_reg>> vars;
  typedef std::variant<on_stack, on_reg> strict_location_t;
  std::array<std::variant<on_stack, on_reg>, reg::non_volatiles.size()> saved;

  typedef std::monostate free;
  typedef register_t save;
  typedef std::variant<free, var, save> content_t;
  std::array<std::variant<free, var, save>, reg::all.size()> regs;
  std::vector<std::variant<free, var, save>> stack;
};

std::ostream &operator<<(std::ostream &os, const context_t::streamable &);
struct offset {
  offset(const size_t s) : s(s) {}
  const size_t s;
};
std::ostream &operator<<(std::ostream &os, const offset &o) {
  if (o.s) {
    os << "+" << o.s;
  }
  return os;
}

std::unordered_set<var> scope_setup_destroys(scope &s, std::unordered_set<var> to_destroy);

// Data structures:
//1. Context
//  a. mapping variable to their locations of type | Not_yet_created | Dead | Constant of value | Global of string | Stack of int | Register of reg

//  b. mapping register to the variable in them, and mapping stack to variable in them
//2. Last use
//3. LCO

bool unroll_last_copy(scope &s);

context_t scope_compile_rec(scope &s, std::ostream &os, context_t c, bool last_call);

}

#endif //COMPILERS_BML_LIB_IR_IR_H_
