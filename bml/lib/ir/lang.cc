#include <lang.h>
#include <cassert>
#include <unordered_set>
#include <map>
#include <list>

namespace std {
template<>
struct hash<ir::lang::var> {
  std::size_t operator()(const ir::lang::var &k) const {
    return k.id;
  }
};

}

template<class... Ts>
struct overloaded : Ts ... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace ir::lang {

std::unordered_map<uint64_t, std::string> var::maybe_names = {{0, "__argv"}};

rhs_expr::memory_access var::operator*() const {
  return rhs_expr::memory_access{.base = *this, .block_offset = 0};
}
rhs_expr::memory_access var::operator[](size_t idx) const {
  return rhs_expr::memory_access{.base = *this, .block_offset = idx};
}
var::operator rhs_expr::copy() const {
  return rhs_expr::copy{.v=*this};
}
instruction::assign var::assign(rhs_expr::t &&src) const {
  return instruction::assign{.dst = *this, .src = std::move(src)};
}

scope &scope::operator<<(instruction::t &&i) {
  push_back(std::move(i));
  return *this;
}

void scope::push_back(instruction::t &&i) {
  body.push_back(std::move(i));
}

enum register_t {
  rbx = 0, rbp = 1, rsi = 2, r12 = 3, r13 = 4, r14 = 5, r15 = 6, // non-volatile (callee-saved)
  rdi = 7, rax = 8, rcx = 9, rdx = 10, r8 = 11, r9 = 12, r10 = 13, r11 = 14, // volatiles (caller-saved)
  // rsp is not managed this way// non-volatile (callee-saved)
};
namespace reg {
bool is_volatile(register_t r) {
  switch (r) {

    case rax:
    case rcx:
    case rdx:
    case r8:
    case r9:
    case r10:
    case r11:
    case rdi:return true;

    case rbx:
    case rbp:
    case rsi:
    case r12:
    case r13:
    case r14:
    case r15:return false;
  }
  THROW_INTERNAL_ERROR
}
std::string_view to_string(register_t r) {
  switch (r) {

    case rax:return "rax";
    case rcx:return "rcx";
    case rdx:return "rdx";
    case r8:return "r8";
    case r9:return "r9";
    case r10:return "r10";
    case r11:return "r1";
    case rdi:return "rdi";
    case rbx:return "rbx";
    case rbp:return "rbp";
    case rsi:return "rsi";
    case r12:return "r12";
    case r13:return "r13";
    case r14:return "r14";
    case r15:return "r15";
  }
  THROW_INTERNAL_ERROR
}
constexpr auto all = util::make_array(rax, rcx, rdx, r8, r9, r10, r11, rbx, rbp, rdi, rsi, r12, r13, r14, r15);
constexpr auto non_volatiles = util::make_array(rbx, rbp, rsi, r12, r13, r14, r15);
};

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

template<typename T, typename V>
bool contains(const V &v, const T &k) {
  for (const auto &x : v)if (x == k)return true;
  return false;
}

template<typename T>
struct lru_list {
 private:
  std::list<T> list;
  std::unordered_map<T, typename std::list<T>::iterator> its;
 public:
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

struct context_t {
  //TODO: should store destruction state of variables (should this be here or not?)
  std::unordered_map<var, location_t> vars;
  std::array<act_location_t, reg::non_volatiles.size()> saved;
  std::array<content_t, reg::all.size()> regs;
  std::vector<content_t> stack;
  lru_list<register_t> lru;
  static context_t merge(context_t c1, std::ostream &os1, context_t c2, std::ostream &os2) {
    //remove non-intersected deads
    //assert everything outside intersection ij only deads

    std::erase_if(c1.vars, [&](const auto &k_v) {
      if (c2.vars.contains(k_v.first))return false;
      assert(var_loc::is_dead(k_v.second));
      return true;
    });
    std::erase_if(c1.vars, [&](const auto &k_v) {
      if (c2.vars.contains(k_v.first))return false;
      assert(var_loc::is_dead(k_v.second));
      return true;
    });

    bool stored_mismatch = false;
    for (const auto&[k, v] : c1.vars) {
      assert(c2.vars.contains(k));
      if (var_loc::is_stored(v) != var_loc::is_stored(c2.vars.at(k))) {
        assert(!stored_mismatch);
        stored_mismatch = true;
        THROW_UNIMPLEMENTED
      }
    }
    //strategy: make c1 equal to c2
    //1. stack
    if (c1.stack != c2.stack) {
      THROW_UNIMPLEMENTED
    }

    assert(c1.stack == c2.stack);

    //2. regs
    if (c1.regs != c2.regs) {
      for (auto r : reg::all) {
        while (!content_opts::is_free(c2.regs[r]) && !(c1.regs[r] == c2.regs[r])) {

          if (content_opts::is_free(c1.regs[r])) {
            // use move
            std::visit(overloaded{
                [](const content_opts::free &) { THROW_INTERNAL_ERROR },
                [&](const content_opts::store_var &v) {
                  assert(c1.vars.contains(v.v));
                  std::visit(overloaded{
                      [](const var_loc::dead &) { THROW_INTERNAL_ERROR },
                      [](const var_loc::unborn &) { THROW_INTERNAL_ERROR },
                      [](const var_loc::global &) { THROW_UNIMPLEMENTED },
                      [](const var_loc::constant &) { THROW_UNIMPLEMENTED },
                      [](const var_loc::on_stack &) { THROW_INTERNAL_ERROR },
                      [&](const var_loc::on_reg &r2) {
                        os1 << "mov " << reg::to_string(r) << ", " << reg::to_string(r2.r) << "; merge context\n";
                        std::swap(c1.regs[r], c1.regs[r2.r]);
                        c1.vars[v.v] = var_loc::on_reg{.r = r};
                      },
                  }, c1.vars.at(v.v));
                },

                [](const content_opts::saved_reg &) { THROW_UNIMPLEMENTED },
            }, c2.regs[r]);
          } else {
            THROW_UNIMPLEMENTED //use xchg
          }
        }
      }
    }

    assert(c1.regs == c2.regs);





    //TODO: intersection must match on unborn, deads, union of {constant,global,on_stack,on_reg}
    //TODO: at most one variable can change from true store {on_stack,on_reg} to {constant,global}

    assert(c1.stack == c2.stack);
    assert(c1.regs == c2.regs);
    assert(c1.vars == c2.vars);
    return c1;
  }
  bool assert_consistency() const {
    std::string debug_str = debug();
    //regs
    for (auto r : reg::all) {
      std::visit(overloaded{
          [&](const content_opts::saved_reg &s) {
            assert(s.r < saved.size());
            assert(saved.at(s.r) == act_location_t{var_loc::on_reg{.r=r}});
          },
          [](const content_opts::free &) {},
          [&](const content_opts::store_var &v) {
            assert(vars.at(v.v) == location_t{var_loc::on_reg{.r=r}});
          },
      }, regs[r]);
    }
    //stack
    for (size_t i = 0; i < stack.size(); ++i) {
      std::visit(overloaded{
          [&](const content_opts::saved_reg &s) {
            assert(saved.at(s.r) == act_location_t{var_loc::on_stack{.p=i}});
          },
          [](const content_opts::free &) {},
          [&](const content_opts::store_var &v) {
            assert(vars.at(v.v) == location_t{var_loc::on_stack{.p=i}});
          },
      }, stack[i]);
    }
    //vars
    for (const auto &v_s : vars) {
      const var v = v_s.first;
      std::visit(overloaded{
          [](const var_loc::unborn &) {},
          [](const var_loc::dead &) {},
          [](const var_loc::constant &) {},
          [](const var_loc::global &) {},
          [&](const var_loc::on_stack &st) {
            assert(stack[st.p] == content_t{content_opts::store_var{.v=v}});
          },
          [&](const var_loc::on_reg &r) {
            assert(regs[r.r] == content_t{content_opts::store_var{.v=v}});
          },
      }, v_s.second);
    }

    //saved
    for (auto r : reg::non_volatiles) {
      std::visit(overloaded{
          [&](const var_loc::on_reg &reg) {
            if (!(regs[reg.r] == content_t{content_opts::saved_reg{.r=r}})) {
              assert(false);
            }
            assert(regs[reg.r] == content_t{content_opts::saved_reg{.r=r}});
          },
          [&](const var_loc::on_stack &st) {
            assert(stack[st.p] == content_t{content_opts::saved_reg{.r=r}});
          }
      }, saved[r]);
    }

  }
  void cerr_debug() const {
   std::cerr << debug() << std::endl;
  }
  std::string debug() const {
    std::stringstream s;
    s << "Registers:\n";
    for (auto r : reg::all) {
      s << reg::to_string(r) << ": ";
      content_opts::print(regs[r], s);
      s << "\n";
    }
    s << "\nStack:\n";
    for (size_t i = 0; i < stack.size(); ++i) {
      s << "[" << i << "]: ";
      content_opts::print(stack[i], s);
      s << "\n";
    }
    s << "\n ------------- \n";
    s << "Vars:\n";
    for (const auto&[v, l] : vars) {
      v.print(s);
      s << ": ";
      var_loc::print(l, s);
      s << "\n";
    }
    s << "\nSaves:\n";
    for (auto r : reg::non_volatiles) {
      s << reg::to_string(r) << ": ";
      var_loc::print(saved[r], s);
      s << "\n";
    }

    return s.str();
  }
  context_t() {
    for (auto r : reg::all)regs[r] = reg::is_volatile(r) ? content_t(content_opts::free{}) : content_opts::saved_reg{r};
    for (auto r : reg::non_volatiles)saved[r] = var_loc::on_reg{.r=r};
    regs[rdi] = content_t(content_opts::store_var{.v=argv_var});
    vars[argv_var] = var_loc::on_reg{.r=rdi};

    for (auto r : reg::all)if (r != rdi && reg::is_volatile(r))lru.push_back(r);
    for (auto r : reg::all)if (r != rdi && !reg::is_volatile(r))lru.push_back(r);
    lru.push_back(rdi);
    assert_consistency();
  };
  void clean(std::ostream &os) {
    assert_consistency();
    while (!stack.empty()) {
      using namespace content_opts;
      std::visit(overloaded{
          [&](free &) {
            os << "add rsp, 8\n";
          },
          [&](saved_reg &sr) {
            os << "pop " << reg::to_string(sr.r) << "\n; restore callee register";
          },
          [](store_var &) { THROW_INTERNAL_ERROR },
      }, stack.back());
      stack.pop_back();
    }
    for (auto r : reg::all)
      if (!reg::is_volatile(r)) {
        assert(regs[r] == content_t(content_opts::saved_reg{r}));
      };
    assert_consistency();
  }
  void destroy(var v, std::ostream &os) {
    assert_consistency();
    assert(vars.contains(v));
    std::visit<void>(overloaded{
        [](const var_loc::unborn &) { THROW_INTERNAL_ERROR },
        [](const var_loc::dead &) { THROW_INTERNAL_ERROR },
        [](const var_loc::constant &) {  },
        [](const var_loc::global &) { THROW_UNIMPLEMENTED },
        [](const var_loc::on_stack &) { THROW_UNIMPLEMENTED },
        [&](const var_loc::on_reg &r) {
          //TODO: might have to go throuh
          regs[r.r] = content_opts::free{};
          lru.bring_front(r.r);
        },
    }, vars.at(v));
    vars.at(v) = var_loc::dead{};
    assert_consistency();
  }
  void destroy(const std::vector<var> &vars, std::ostream &os) {
    for (var v : vars)destroy(v, os);
  }
  void declare_const(var v, uint64_t value) {
    assert_consistency();
    assert(!vars.contains(v));
    vars[v] = var_loc::constant{.v = value};
    assert_consistency();
  }
  void declare_move(var v, var m) {
    assert_consistency();
    assert(!vars.contains(v));
    assert(vars.contains(m));
    vars[v] = vars[m];
    vars[m] = var_loc::dead{};
    std::visit(overloaded{
        [](const var_loc::unborn &) { THROW_INTERNAL_ERROR },
        [](const var_loc::dead &) { THROW_INTERNAL_ERROR },
        [](const var_loc::constant &c) {},
        [](const var_loc::global &) {},
        [&](const var_loc::on_stack &s) {
          stack[s.p] = content_opts::store_var{.v=v};
        },
        [&](const var_loc::on_reg &r) {
          regs[r.r] = content_opts::store_var{.v=v};
          lru.bring_back(r.r);
        },
    }, vars[v]);
    assert_consistency();
  }

  void declare(var v, std::ostream &os, bool in_reg = false) {
    assert_consistency();
    register_t r = lru.front();
    if (!std::holds_alternative<content_opts::free>(regs[r])) {
      THROW_UNIMPLEMENTED
      //need to save the content somewhere
    }
    lru.bring_back(r);
    regs[r] = content_opts::store_var{.v=v};
    vars[v] = var_loc::on_reg{.r=r};
    assert_consistency();
  }
  void load_in_specific_reg(var v, register_t r, std::ostream &os) {
    assert_consistency();
    assert(vars.contains(v));

    if (std::holds_alternative<content_opts::store_var>(regs.at(r)) && std::get<content_opts::store_var>(regs.at(r)).v == v) {
      //variable is already there
      assert_consistency();
      return;
    }
    std::visit(overloaded{
        [](content_opts::free &) {
          // no problem
        },
        [](content_opts::saved_reg &) {
          THROW_UNIMPLEMENTED
        },
        [](content_opts::store_var &) { THROW_UNIMPLEMENTED },
    }, regs.at(r));
    assert(std::holds_alternative<content_opts::free>(regs.at(r)));

    std::visit<void>(overloaded{
        [](const var_loc::unborn &) { THROW_INTERNAL_ERROR },
        [](const var_loc::dead &) { THROW_INTERNAL_ERROR },
        [&](const var_loc::constant &c) {
          os << "mov " << reg::to_string(r) << ", " << c.v << "\n";
          regs[r] = content_opts::store_var{.v=v};
          vars[v] = var_loc::on_reg{.r=r};
        },
        [](const var_loc::global &) {
          // We should do nothing
          THROW_UNIMPLEMENTED
        },
        [](const var_loc::on_stack &s) {
          //Move it
          THROW_UNIMPLEMENTED
        },
        [&](const var_loc::on_reg &rnow) {
          assert(rnow.r != r);
          //If it was already in reg, then we exited
          os << "mov " << reg::to_string(r) << ", " << reg::to_string(rnow.r) << "\n";

          regs[r] = content_opts::store_var{.v=v};
          regs[rnow.r] = content_opts::free{};
          vars[v] = var_loc::on_reg{.r=r};
        },
    }, vars.at(v));
    assert_consistency();
  }
  void load_in_reg(var v, std::ostream &os) {
    assert_consistency();
    if (std::holds_alternative<var_loc::on_reg>(vars[v])) {
      //already in reg - but let's enhance its life
      lru.bring_back(std::get<var_loc::on_reg>(vars[v]).r);
      return;
      assert_consistency();
    }

    register_t r = lru.front();
    if (!std::holds_alternative<content_opts::free>(regs[r])) {
      THROW_UNIMPLEMENTED
      //need to save the content somewhere
    }
    lru.bring_back(r);
    regs[r] = content_opts::store_var{.v=v};
    std::visit<void>(overloaded{
        [](const var_loc::unborn &) { THROW_INTERNAL_ERROR },
        [](const var_loc::dead &) { THROW_INTERNAL_ERROR },
        [](const var_loc::constant &) {
          // We should do nothing
          THROW_UNIMPLEMENTED
        },
        [](const var_loc::global &) {
          // We should do nothing
          THROW_UNIMPLEMENTED
        },
        [](const var_loc::on_stack &s) {
          //Move it
          THROW_UNIMPLEMENTED
        },
        [&](const var_loc::on_reg &r) {
          //If it was already in reg, then we exited
          THROW_INTERNAL_ERROR
        },
    }, vars.at(v));
    vars[v] = var_loc::on_reg{.r=r};
    assert_consistency();
  }
  void retrieve(var v, std::ostream &os) const {
    assert_consistency();
    assert(vars.contains(v));
    std::visit<void>(overloaded{
        [](const var_loc::unborn &) { THROW_INTERNAL_ERROR },
        [](const var_loc::dead &) { THROW_INTERNAL_ERROR },
        [&](const var_loc::constant &c) {
          os << c.v;
        },
        [&](const var_loc::global &g) {
          os << g.name;
        },
        [&](const var_loc::on_stack &s) {
          os << "qword [rsp";
          if (s.p < stack.size() - 1) {
            os << "+" << (stack.size() - 1 - s.p) * 8;
          }
          os << "]";
        },
        [&](const var_loc::on_reg &r) {
          os << reg::to_string(r.r);
        },
    }, vars.at(v));
    assert_consistency();
  }
};

std::unordered_set<var> scope_setup_destroys(scope &s, std::unordered_set<var> to_destroy) {
  s.destroys.clear();
  for (auto &i : s.body)
    if (std::holds_alternative<instruction::assign>(i)) {
      to_destroy.insert(std::get<instruction::assign>(i).dst);
    }

  auto destroy_here = [&](var v, size_t i) {
    if (to_destroy.contains(v)) {
      s.destroys[i + 1].push_back(v);
      to_destroy.erase(v);
    }
  };
  to_destroy.erase(s.ret);
  for (int i = s.body.size() - 1; i >= 0; --i)
    std::visit(overloaded{
        [&](instruction::assign &a) {
          std::visit(overloaded{
              [](rhs_expr::constant &) {},
              [](rhs_expr::global &) {},
              [&](rhs_expr::copy &ce) {
                destroy_here(ce.v, i);
              },
              [&](rhs_expr::memory_access &ma) {
                destroy_here(ma.base, i);
              },
              [](rhs_expr::malloc &m) {
              },
              [&](rhs_expr::apply_fn &a) {
                destroy_here(a.f, i);
                destroy_here(a.x, i);
              },
              [&](rhs_expr::branch &b) {
                auto s1 = scope_setup_destroys(b->jmp_branch, to_destroy);
                auto s2 = scope_setup_destroys(b->nojmp_branch, to_destroy);
                std::erase_if(s1, [&](var v) {
                  if (s2.contains(v))return false;
                  b->jmp_branch.destroys[0].push_back(v);
                  return true;
                });
                std::erase_if(s2, [&](var v) {
                  if (s1.contains(v))return false;
                  b->nojmp_branch.destroys[0].push_back(v);
                  return true;
                });
                assert(s1 == s2);
                to_destroy = s1;
              },
              [&](rhs_expr::unary_op &b) { destroy_here(b.x, i); },
              [&](rhs_expr::binary_op &b) {
                destroy_here(b.x1, i);
                destroy_here(b.x2, i);
              }
          }, a.src);
          destroy_here(a.dst, i); // only if var never used
        },
        [&](instruction::write_uninitialized_mem &w) {
          destroy_here(w.base, i);
          destroy_here(w.src, i);
        },
        [&](instruction::cmp_vars &c) {
          destroy_here(c.v1, i);
          destroy_here(c.v2, i);
          //ATCHUNG! after a cmp, destroys can potentially unvalidate the flags.
          //However the content are likely immediate, so no actual deallocation will take place - lets ensure that
        },
    }, s.body[i]);
  return to_destroy;
}

// Data structures:
//1. Context
//  a. mapping variable to their locations of type | Not_yet_created | Dead | Constant of value | Global of string | Stack of int | Register of reg

//  b. mapping register to the variable in them, and mapping stack to variable in them
//2. Last use
//3. LCO

bool unroll_last_copy(scope &s) {
  if (s.body.empty())return false;
  if (!std::holds_alternative<instruction::assign>(s.body.back()))return false;
  instruction::assign &last = std::get<instruction::assign>(s.body.back());
  if (last.dst != s.ret)return false;
  if (!std::holds_alternative<rhs_expr::copy>(last.src))return false;
  s.ret = std::get<rhs_expr::copy>(last.src).v;
  s.body.pop_back();
  return true;
}

context_t scope_compile_rec(scope &s, std::ostream &os, context_t c, bool last_call) {
  static size_t branch_id_factory = 0;
  while (unroll_last_copy(s)); //TODO: unroll all better

  bool need_to_return = last_call;
  if (s.destroys.contains(0))c.destroy(s.destroys.at(0), os);
  for (size_t i = 0; i < s.body.size(); ++i) {
    std::visit(overloaded{
        [&](instruction::assign &a) {
          if (last_call && (i == s.body.size() - 1) &&
              (std::holds_alternative<rhs_expr::apply_fn>(a.src) || std::holds_alternative<rhs_expr::branch>(a.src))) {
            need_to_return = false;
            assert(a.dst == s.ret);
            std::visit(overloaded{
                [&](rhs_expr::constant &ce) {
                  //cannot be here
                  THROW_INTERNAL_ERROR
                },
                [](rhs_expr::global &) {  //cannot be here
                  THROW_INTERNAL_ERROR
                },
                [](rhs_expr::copy &) {  //cannot be here
                  THROW_INTERNAL_ERROR
                },
                [&](rhs_expr::memory_access &ma) {//cannot be here
                  THROW_INTERNAL_ERROR
                },
                [](rhs_expr::malloc &) { THROW_UNIMPLEMENTED },
                [](rhs_expr::apply_fn &) { THROW_UNIMPLEMENTED },
                [&](rhs_expr::branch &b) {
                  size_t this_branch = ++branch_id_factory;
                  os << b->ops_to_string() << " .L" << this_branch << "\n";
                  scope_compile_rec(b->nojmp_branch, os, c, true);
                  os << ".L" << this_branch << "\n";
                  scope_compile_rec(b->jmp_branch, os, c, true);
                },
                [](rhs_expr::unary_op &) { THROW_UNIMPLEMENTED },
                [](rhs_expr::binary_op &) { THROW_UNIMPLEMENTED }
            }, a.src);
          } else {
            std::visit(overloaded{
                [&](rhs_expr::constant &ce) {
                  c.declare_const(a.dst, ce.v);
                },
                [](rhs_expr::global &) {
                  THROW_UNIMPLEMENTED
                },
                [&](rhs_expr::copy &r) {
                  if (s.destroys.contains(i + 1)) {
                    assert(s.destroys.at(i + 1).size() == 1);//destroying the current rhs
                    assert(s.destroys.at(i + 1).front() == r.v);
                    s.destroys.erase(i + 1);
                    c.declare_move(a.dst, r.v);
                  } else {
                    THROW_UNIMPLEMENTED
                  }
                },
                [&](rhs_expr::memory_access &ma) {
                  c.load_in_reg(ma.base, os);
                  c.declare(a.dst, os, true);

                  os << "mov ";
                  c.retrieve(a.dst, os);
                  os << ", qword [";
                  c.retrieve(ma.base, os);
                  if (ma.block_offset) {
                    os << "+" << ma.block_offset * 8;
                  }
                  os << "]\n";
                },
                [](rhs_expr::malloc &) { THROW_UNIMPLEMENTED },
                [](rhs_expr::apply_fn &) { THROW_UNIMPLEMENTED },
                [&](rhs_expr::branch &b) {

                  size_t this_branch = ++branch_id_factory;
                  size_t this_branch_end = ++branch_id_factory;
                  os << b->ops_to_string() << " .L" << this_branch << "\n";
                  context_t c1 = scope_compile_rec(b->nojmp_branch, os, c, false);
                  c1.declare_move(a.dst, b->nojmp_branch.ret);
                  std::stringstream jb;
                  context_t c2 = scope_compile_rec(b->jmp_branch, jb, c, false);
                  c2.declare_move(a.dst, b->jmp_branch.ret);
                  c = context_t::merge(c1, os, c2, jb);
                  os << "jmp .L" << this_branch_end << "\n";
                  os << ".L" << this_branch << "\n";
                  os << jb.str();
                  os << ".L" << this_branch_end << "\n";
                },
                [&](rhs_expr::unary_op &) {
                  THROW_UNIMPLEMENTED
                },
                [&](rhs_expr::binary_op &b) {
                  const bool operator_commutative = rhs_expr::binary_op::is_commutative(b.op);
                  if (s.destroys.contains(i + 1) && operator_commutative && s.destroys.at(i + 1).front() != b.x1)std::swap(b.x1, b.x2);
                  if (s.destroys.contains(i + 1) && contains(s.destroys.at(i + 1), b.x1)) {
                    //variable move
                    os << rhs_expr::binary_op::ops_to_string(b.op) << " ";
                    assert(c.vars.contains((b.x1)));
                    assert(c.vars.contains((b.x2)));
                    c.retrieve(b.x1, os);
                    os << ", ";
                    c.retrieve(b.x2, os);
                    os << "\n";
                    c.declare_move(a.dst, b.x1);
                    s.destroys.at(i + 1).erase(std::find(s.destroys.at(i + 1).begin(), s.destroys.at(i + 1).end(), b.x1));

                  } else {
                    THROW_UNIMPLEMENTED
                  }
                }

            }, a.src);
          };
        },
        [](instruction::write_uninitialized_mem &) { THROW_UNIMPLEMENTED },
        [&](instruction::cmp_vars &cmp) {
          c.load_in_reg(cmp.v1, os);
          c.load_in_reg(cmp.v2, os);
          os << instruction::cmp_vars::ops_to_string(cmp.op) << " ";
          c.retrieve(cmp.v1, os);
          os << ", ";
          c.retrieve(cmp.v2, os);
          os << "\n";
        },
    }, s.body[i]);
    if (need_to_return && s.destroys.contains(i + 1))c.destroy(s.destroys.at(i + 1), os);
  }

  if (need_to_return) {
    //TODO: return value
    c.clean(os);
    c.load_in_specific_reg(s.ret, rax, os);
    os << "ret\n";
  }

  return c;
}

void scope::compile_as_function(std::ostream &os) {
  for (const var &v : scope_setup_destroys(*this, {argv_var}))destroys[0].push_back(v);
  scope_compile_rec(*this, os, context_t{}, true);

}
void scope::print(std::ostream &os, size_t offset) {
  for (auto &it : body) {
    for (int i = offset; i--;)os << "  ";
    std::visit(overloaded{
        [&](instruction::assign &a) {

          a.dst.print(os);
          os << " = ";
          std::visit(overloaded{
              [&](rhs_expr::constant &ce) { os << ce.v; },
              [](rhs_expr::global &) { THROW_UNIMPLEMENTED },
              [&](rhs_expr::copy &ce) { ce.v.print(os); },
              [&](rhs_expr::memory_access &ma) {
                if (ma.block_offset == 0) os << "*";
                ma.base.print(os);
                if (ma.block_offset) os << "[" << ma.block_offset << "]";
              },
              [](rhs_expr::malloc &) { THROW_UNIMPLEMENTED },
              [](rhs_expr::apply_fn &) { THROW_UNIMPLEMENTED },
              [&](rhs_expr::branch &b) {
                os << "if (" <<
                   b->ops_to_string() << "){\n";
                b->nojmp_branch.print(os, offset + 1);
                for (int i = offset; i--;)os << "  ";
                os << "} else {\n";
                b->jmp_branch.print(os, offset + 1);
                for (int i = offset; i--;)os << "  ";
                os << "}";
              },
              [&](rhs_expr::unary_op &) { THROW_UNIMPLEMENTED },
              [&](rhs_expr::binary_op &b) {
                os << rhs_expr::binary_op::ops_to_string(b.op) << " ";
                b.x1.print(os);
                os << " ";
                b.x2.print(os);
              }
          }, a.src);
        },
        [](instruction::write_uninitialized_mem &a) { THROW_UNIMPLEMENTED },
        [&](instruction::cmp_vars &a) {
          os << instruction::cmp_vars::ops_to_string(a.op) << "(";
          a.v1.print(os);
          os << ", ";
          a.v2.print(os);
          os << ")";
        },
    }, it);
    os << ";\n";
  }
  for (int i = offset; i--;)os << "  ";
  os << "return ";
  ret.print(os);
  os << ";\n";
}

}
