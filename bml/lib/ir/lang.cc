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

/*
INSTR_OPS_2(mov)
INSTR_OPS_2(add)
INSTR_OPS_2(cmp)
INSTR_OPS_1(sar)
INSTR_OPS_1(ret)

void scope::push_back(instruction &&i) {
  ops.emplace_back(std::move(i));
}
void scope::push_back(ternary &&t) {
  ops.emplace_back(std::move(t));
}
scope &scope::operator<<(instruction &&i) {
  push_back(std::move(i));
  return *this;
}
scope &scope::operator<<(ternary &&i) {
  push_back(std::move(i));
  return *this;
}
 */
scope &scope::operator<<(instruction::t &&i) {
  push_back(std::move(i));
  return *this;
}

void scope::push_back(instruction::t &&i) {
  body.push_back(std::move(i));
}

enum register_t {
  rax = 0, rcx = 1, rdx = 2, r8 = 3, r9 = 4, r10 = 5, r11 = 6, // volatiles (caller-saved)
  rbx = 7, rbp = 8, rdi = 9, rsi = 10, r12 = 11, r13 = 12, r14 = 13, r15 = 14, // non-volatile (callee-saved)
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
};

namespace var_loc {
struct unborn {};
struct dead {};
struct constant { uint64_t v; };
struct global { std::string name; };
struct on_stack { size_t p; };
struct on_reg { register_t r; };
typedef std::variant<unborn, dead, constant, global, on_stack, on_reg> variant;
};
typedef var_loc::variant location_t;
namespace content_opts {
struct free { bool operator==(const free &) const { return true; }};
struct saved_reg { register_t r; bool operator==(const saved_reg &sr) const { return r == sr.r; }};
struct store_var { var v; bool operator==(const store_var &sv) const { return v == sv.v; }};
typedef std::variant<free, saved_reg, store_var> variant;
};
typedef content_opts::variant content_t;

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
  std::unordered_map<var, location_t> vars;
  std::array<content_t, reg::all.size()> regs;
  std::vector<content_t> stack;
  lru_list<register_t> lru;
  context_t() {
    for (auto r : reg::all)regs[r] = reg::is_volatile(r) ? content_t(content_opts::free{}) : content_opts::saved_reg{r};
    regs[rdi] = content_t(content_opts::store_var{.v=argv_var});
    vars[argv_var] = var_loc::on_reg{.r=rdi};

    for (auto r : reg::all)if (r != rdi && reg::is_volatile(r))lru.push_back(r);
    for (auto r : reg::all)if (r != rdi && !reg::is_volatile(r))lru.push_back(r);
    lru.push_back(rdi);
  };
  void clean(std::ostream &os) {
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

  }
  void destroy(var v, std::ostream &os) {
    assert(vars.contains(v));
    std::visit<void>(overloaded{
        [](const var_loc::unborn &) { THROW_INTERNAL_ERROR },
        [](const var_loc::dead &) { THROW_INTERNAL_ERROR },
        [](const var_loc::constant &) { THROW_UNIMPLEMENTED },
        [](const var_loc::global &) { THROW_UNIMPLEMENTED },
        [](const var_loc::on_stack &) { THROW_UNIMPLEMENTED },
        [&](const var_loc::on_reg &r) {
          //TODO: might have to go throuh
          regs[r.r] = content_opts::free{};
          lru.bring_front(r.r);
        },
    }, vars.at(v));
    vars.at(v) = var_loc::dead{};
  }
  void destroy(const std::vector<var> &vars, std::ostream &os) {
    for (var v : vars)destroy(v, os);
  }
  void declare_const(var v, uint64_t value) {
    assert(!vars.contains(v));
    vars[v] = var_loc::constant{.v = value};
  }
  void declare_move(var v, var m) {
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
        },
    }, vars[v]);
  }

  void declare(var v, std::ostream &os, bool in_reg = false) {
    register_t r = lru.front();
    if (!std::holds_alternative<content_opts::free>(regs[r])) {
      THROW_UNIMPLEMENTED
      //need to save the content somewhere
    }
    lru.bring_back(r);
    regs[r] = content_opts::store_var{.v=v};
    vars[v] = var_loc::on_reg{.r=r};
  }
  void load_in_specific_reg(var v, register_t r, std::ostream &os) {
    if (!vars.contains(v)) {
      assert(vars.contains(v));
    }
    if (std::holds_alternative<content_opts::store_var>(regs.at(r)) && std::get<content_opts::store_var>(regs.at(r)).v == v) {
      //variable is already there
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
          regs[r] = content_opts::store_var{.v=v};
          regs[rnow.r] = content_opts::free{};
          os << "mov " << reg::to_string(r) << ", " << reg::to_string(rnow.r) << "\n";
        },
    }, vars.at(v));
  }
  void load_in_reg(var v, std::ostream &os) {
    if (std::holds_alternative<var_loc::on_reg>(vars[v])) {
      //already in reg - but let's enhance its life
      lru.bring_back(std::get<var_loc::on_reg>(vars[v]).r);
      return;
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

  }
  void retrieve(var v, std::ostream &os) const {
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
  }
};

//remove (and returns) the used vars
std::unordered_set<var> used_vars(instruction::t &i, std::unordered_set<var> &set) {
  return std::visit(overloaded{
      [](instruction::assign &a) -> std::unordered_set<var> {
        return std::visit(overloaded{
            [](rhs_expr::constant &) -> std::unordered_set<var> { THROW_UNIMPLEMENTED },
            [](rhs_expr::global &) -> std::unordered_set<var> { THROW_UNIMPLEMENTED },
            [&](rhs_expr::copy &ce) -> std::unordered_set<var> { THROW_UNIMPLEMENTED },
            [&](rhs_expr::memory_access &ma) -> std::unordered_set<var> { THROW_UNIMPLEMENTED },
            [](rhs_expr::malloc &) -> std::unordered_set<var> { THROW_UNIMPLEMENTED },
            [](rhs_expr::apply_fn &) -> std::unordered_set<var> { THROW_UNIMPLEMENTED },
            [](rhs_expr::branch &) -> std::unordered_set<var> { THROW_UNIMPLEMENTED }
        }, a.src);
      },
      [](instruction::write_uninitialized_mem &) -> std::unordered_set<var> { THROW_UNIMPLEMENTED },
      [](instruction::cmp_vars &) -> std::unordered_set<var> { THROW_UNIMPLEMENTED },
  }, i);
}

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
                  static size_t branch = 0;
                  size_t this_branch = ++branch;
                  os << b->ops_to_string() << " .LC" << this_branch << "\n";
                  scope_compile_rec(b->nojmp_branch, os, c, true);
                  os << ".LC" << this_branch << "\n";
                  scope_compile_rec(b->jmp_branch, os, c, true);
                }
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
                [](rhs_expr::branch &) { THROW_UNIMPLEMENTED }

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
