#include <ir.h>
#include <lang.h>

namespace ir {
using namespace util;
namespace reg {
bool is_volatile(ir::register_t r) {
  switch (r) {

    case rax:
    case rcx:
    case rdx:
    case r8:
    case r9:
    case r10:
    case r11:
    case rdi:
    case rsi:return true;

    case rbx:
    case rbp:
    case r12:
    case r13:
    case r14:
    case r15:return false;
  }
  THROW_INTERNAL_ERROR
}
bool is_non_volatile(register_t r) { return !is_volatile(r); }
std::string_view to_string(register_t r) {
  switch (r) {

    case rax:return "rax";
    case rcx:return "rcx";
    case rdx:return "rdx";
    case r8:return "r8";
    case r9:return "r9";
    case r10:return "r10";
    case r11:return "r11";
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
}
namespace {

template<typename T, typename V>
bool contains(const V &v, const T &k) {
  for (const auto &x : v)if (x == k)return true;
  return false;
}
}
std::ostream &operator<<(std::ostream &os, const context_t::streamable &s) {
  s.context.retrieve(s.v, os);
  return os;
}
std::ostream &operator<<(std::ostream &os, const offset &o) {
  if (o.s) {
    os << "+" << o.s;
  }
  return os;
}
context_t::streamable::streamable(const context_t &context, var v) : context(context), v(v) {}
std::unordered_set<var> scope_setup_destroys(scope &s, std::unordered_set<var> to_destroy) {
  s.destroys.clear();
  s.destroys.resize(s.body.size() + 1, {});
  for (auto &i : s.body)
    if (std::holds_alternative<instruction::assign>(i)) {
      const auto &ia = std::get<instruction::assign>(i);
      if (!std::holds_alternative<rhs_expr::constant>(ia.src) && !std::holds_alternative<rhs_expr::global>(ia.src))to_destroy.insert(ia.dst);
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
  c.destroy(s.destroys.at(0), os);
  for (size_t i = 0; i < s.body.size(); ++i) {

    std::vector<var> &destroys = s.destroys.at(i + 1);
    std::visit(overloaded{
        [&](instruction::assign &a) {
          if (last_call && (i == s.body.size() - 1) && (std::holds_alternative<rhs_expr::apply_fn>(a.src) || std::holds_alternative<rhs_expr::branch>(a.src))) {
            need_to_return = false;
            assert(a.dst == s.ret);
            std::visit(overloaded{
                [](const std::variant<rhs_expr::constant, rhs_expr::global, rhs_expr::copy, rhs_expr::memory_access, rhs_expr::unary_op, rhs_expr::binary_op, rhs_expr::malloc> &) {
                  THROW_INTERNAL_ERROR
                },
                [&](rhs_expr::apply_fn &fun) {
                  assert(destroys.size() == 2);
                  assert(contains(destroys, fun.f));
                  assert(contains(destroys, fun.x));
                  destroys.clear();
                  c.return_clean({{fun.f, rsi}, {fun.x, rdi}}, os);
                  os << "jmp apply_fn\n"; //TODO avoid jump
                },
                [&](rhs_expr::branch &b) {
                  size_t this_branch = ++branch_id_factory;
                  os << b->ops_to_string() << " .L" << this_branch << "\n";
                  scope_compile_rec(b->nojmp_branch, os, c, true);
                  os << ".L" << this_branch << "\n";
                  scope_compile_rec(b->jmp_branch, os, c, true);
                }
            }, a.src);
          } else {
            std::visit(overloaded{
                [&](const rhs_expr::constant &ce) {
                  c.declare_const(a.dst, ce.v);
                },
                [&](const rhs_expr::global &g) {
                  c.declare_global(a.dst, g.name);
                },
                [&](rhs_expr::copy &r) {
                  if (!destroys.empty()) {
                    assert(destroys.size() == 1);//destroying the current rhs
                    assert(destroys.front() == r.v);
                    destroys.clear();
                    c.declare_move(a.dst, r.v);
                  } else {
                    std::cerr << "Variable copy - This shouldn't be happening" << std::endl;
                    c.declare_free(a.dst, os);
                    c.make_non_both_mem(a.dst, r.v, os);
                    os << "mov " << c.at(a.dst) << ", " << c.at(r.v) << "\n";
                  }
                },
                [&](rhs_expr::memory_access &ma) {
                  c.declare_free(a.dst, os);
                  c.make_both_non_mem(ma.base, a.dst, os);
                  os << "mov " << c.at(a.dst) << ", qword [" << c.at(ma.base) << offset(ma.block_offset * 8) << "]\n";
                },
                [&](rhs_expr::malloc &m) {
                  c.call_clean({}, os);
                  os << "mov rsi, " << m.size * 8 << " \n";
                  c.declare_in(a.dst, rax);
                },
                [&](rhs_expr::apply_fn &fun) {
                  var f = fun.f, x = fun.x;
                  std::vector<std::pair<var, register_t>> moved;
                  std::vector<std::pair<var, register_t>> copied;
                  if (contains(destroys, f)) {
                    moved.emplace_back(f, rsi);
                    destroys.erase(std::find(destroys.begin(), destroys.end(), f));
                  } else copied.emplace_back(f, rsi);
                  if (contains(destroys, x)) {
                    moved.emplace_back(x, rdi);
                    destroys.erase(std::find(destroys.begin(), destroys.end(), x));
                  } else copied.emplace_back(x, rdi);
                  c.call_clean(moved, os);
                  c.call_copy(copied, os);
                  os << "call apply_fn\n"; //TODO: avoid additional call, traverse it here
                  c.declare_in(a.dst, rax);
                },
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
                  //TODO: optimize jump structure for when either of the body is empty
                },
                [&](rhs_expr::unary_op &u) {
                  os << " ;";
                  a.dst.print(os);
                  os << " = " << rhs_expr::unary_op::ops_to_string(u.op) << "(";
                  u.x.print(os);
                  os << ")\n";
                  const bool destroy_arg = !destroys.empty();
                  if (destroy_arg) {
                    assert(destroys.size() == 1);
                    assert(destroys.front() == u.x);
                    destroys.clear();
                    c.make_non_mem(u.x, os);
                  } else {
                    c.declare_free(a.dst, os);
                    c.make_non_both_mem(a.dst, u.x, os);
                  }

                  rhs_expr::unary_op::compile(os, u.op, c.retrieve_to_string(u.x), c.retrieve_to_string(destroy_arg ? u.x : a.dst));
                  if (destroy_arg)c.declare_move(a.dst, u.x);
                },
                [&](rhs_expr::binary_op &b) {
                  os << " ;";
                  a.dst.print(os);
                  os << " = " << rhs_expr::binary_op::ops_to_string(b.op) << "(";
                  b.x1.print(os);
                  os << ", ";
                  b.x2.print(os);
                  os << ")\n";
                  const bool operator_commutative = rhs_expr::binary_op::is_commutative(b.op);
                  if (!destroys.empty() && operator_commutative && destroys.front() != b.x1)std::swap(b.x1, b.x2);
                  if (contains(destroys, b.x1)) {
                    //variable move
                    assert(c.contains((b.x1)));
                    assert(c.contains((b.x2)));
                    c.make_non_both_mem(b.x1, b.x2, os);
                    os << rhs_expr::binary_op::ops_to_string(b.op) << " " << c.at(b.x1) << ", " << c.at(b.x2) << "\n";
                    c.declare_move(a.dst, b.x1);
                    destroys.erase(std::find(destroys.begin(), destroys.end(), b.x1));
                  } else {
                    c.declare_copy(a.dst, b.x1, os);
                    c.make_non_both_mem(a.dst, b.x2, os);
                    os << rhs_expr::binary_op::ops_to_string(b.op) << " " << c.at(a.dst) << ", " << c.at(b.x2) << "\n";
                  }
                }

            }, a.src);
          };
        },
        [&](instruction::write_uninitialized_mem &m) {
          c.make_both_non_mem(m.src, m.base, os);
          os << "mov qword [" << c.at(m.base) << offset(m.block_offset * 8) << "], " << c.at(m.src) << "\n";
        },
        [&](instruction::cmp_vars &cmp) {
          c.make_non_both_mem(cmp.v1, cmp.v2, os);
          //TODO: assert trivially destructible
          os << instruction::cmp_vars::ops_to_string(cmp.op) << " " << c.at(cmp.v1) << ", " << c.at(cmp.v2) << "\n";
        },
    }, s.body[i]);
    if (need_to_return) {
      c.destroy(destroys, os);
    } else {
      assert(destroys.empty());
    }
  }

  if (need_to_return) {
    c.return_clean({{s.ret, rax}}, os);
    os << "ret\n";
  }

  return c;
}

void scope::compile_as_function(std::ostream &os) {
  //TODO: destroyability analysis
  for (const var &v : scope_setup_destroys(*this, {argv_var}))destroys[0].push_back(v);
  scope_compile_rec(*this, os, context_t{}, true);

}
/*
namespace old {
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
  context_t(const context_t &) = default;
  context_t(context_t &&) = default;
  context_t &operator=(const context_t &) = default;
  context_t &operator=(context_t &&o) = default;
  void clean(std::ostream &os) {
    //after this, stack is empty and non-volatile register are in ori
    assert_consistency();
    while (!stack.empty()) {
      using namespace content_opts;
      if (std::visit(overloaded{
          [&](free &) {
            os << "add rsp, 8\n";
            return true;
          },
          [&](saved_reg sr) {
            return std::visit(overloaded{
                [&](free &) {
                  os << "pop " << reg::to_string(sr.r) << " ; restore callee register\n";
                  regs[sr.r] = sr;
                  saved[sr.r] = var_loc::on_reg{.r=sr.r};
                  return true;
                },
                [&](saved_reg sr2) {
                  os << "xchg qword [rsp], " << reg::to_string(sr.r) << " ; restore callee register\n";
                  std::swap(stack.back(), regs[sr.r]);
                  std::swap(saved[sr.r], saved[sr2.r]);
                  return false;
                },
                [&](store_var sv) {
                  os << "xchg qword [rsp], " << reg::to_string(sr.r) << " ; restore callee register\n";
                  std::swap(stack.back(), regs[sr.r]);
                  saved[sr.r] = var_loc::on_reg{.r=sr.r};
                  vars[sv.v] = var_loc::on_stack{.p = stack.size() - 1};
                  return false;
                },
            }, regs[sr.r]);
          },
          [&](store_var s) {
            while (!reg::is_volatile(lru.front()))lru.bring_back(lru.front());
            auto r = lru.front();
            lru.bring_back(r);
            assert(reg::is_volatile(r));
            assert(is_free(var_loc::on_reg{.r=r}));
            os << "pop " << reg::to_string(r) << "\n";
            std::swap(stack.back(), regs[r]);
            vars[s.v] = var_loc::on_reg{.r=r};
            return true;
          },
      }, stack.back()))
        stack.pop_back();
    }
    for (auto r : reg::all)
      if (!reg::is_volatile(r)) {
        assert(regs[r] == content_t(content_opts::saved_reg{r}));
      };
    assert_consistency();
  }
  void destroy(var v, std::ostream &os) {
    os << "; destroying var ";
    v.print(os);
    os << " \n";
    assert_consistency();
    assert(vars.contains(v));
    std::visit<void>(overloaded{
        [](const var_loc::unborn &) { THROW_INTERNAL_ERROR },
        [](const var_loc::dead &) { THROW_INTERNAL_ERROR },
        [](const var_loc::constant &) {},
        [](const var_loc::global &) { THROW_UNIMPLEMENTED },
        [&](const var_loc::on_stack &s) {
          //TODO: might have to go through
          stack[s.p] = content_opts::free{};
          size_t sfree = 0;
          while (!stack.empty() && content_opts::is_free(stack.back())) {
            ++sfree;
            stack.pop_back();
          }
          if (sfree)os << "add rsp, " << sfree * 8 << " ; reclaiming stack space\n";
        },
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
  void declare_global(var v, std::string_view value) {
    assert_consistency();
    assert(!vars.contains(v));
    vars[v] = var_loc::global{.name = std::string(value)};
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
    if (in_reg) {
      register_t r = free_reg(os);
      regs[r] = content_opts::store_var{.v=v};
      vars[v] = var_loc::on_reg{.r=r};
    } else { THROW_UNIMPLEMENTED }
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
 private:
  bool is_free(var_loc::act_variant l) {
    return std::visit(overloaded{
        [&](var_loc::on_stack s) {
          assert(s.p < stack.size());
          return content_opts::is_free(stack[s.p]);
        },
        [&](var_loc::on_reg r) {
          return content_opts::is_free(regs[r.r]);
        }}, l);
  }
  void xchg(var_loc::act_variant dst, var_loc::act_variant src, std::ostream &os) {

    std::visit(overloaded{
        [](var_loc::on_stack) {

        }, [](var_loc::on_reg) {

        }}, dst);
    THROW_WORK_IN_PROGRESS
  }
  void move(var_loc::act_variant dst, var_loc::variant src, std::ostream &os) {
    assert(is_free(dst));

    return xchg(dst, src, os);
  }
 public:
  register_t free_reg(std::ostream &os, bool avoid_volatiles = false) {
    assert_consistency();
    register_t r = avoid_volatiles ? lru.front_if(reg::is_non_volatile) : lru.front();
    lru.bring_back(r);
    std::visit(overloaded{
        [](content_opts::free) {},
        [&](content_opts::saved_reg s) {
          if (auto it = std::find_if(stack.begin(), stack.end(), content_opts::is_free);it == stack.end()) {
            os << "push " << reg::to_string(r) << " ; saving caller register\n";
            saved[s.r] = var_loc::on_stack{.p = stack.size()};
            stack.emplace_back(s);
          } else {
            size_t idx = std::distance(it, stack.end()) - 1;
            os << "qword [rsp+" << idx * 8 << "], " << reg::to_string(r) << " ; saving caller register\n";
            saved[s.r] = var_loc::on_stack{.p = idx};
            stack[idx] = s;
          }
        },
        [&](content_opts::store_var s) {
          if (auto it = std::find_if(stack.begin(), stack.end(), content_opts::is_free);it == stack.end()) {
            os << "push " << reg::to_string(r) << " ; saving variable ";
            s.v.print(os);
            os << "\n";
            vars[s.v] = var_loc::on_stack{.p = stack.size()};
            stack.emplace_back(s);
          } else {
            size_t idx = std::distance(stack.begin(), it);
            os << "mov qword [rsp+" << (stack.size() - idx - 1) * 8 << "], " << reg::to_string(r) << " ; saving variable ";
            s.v.print(os);
            os << "\n";
            vars[s.v] = var_loc::on_stack{.p = idx};
            stack[idx] = s;
          }

        },
    }, regs[r]);
    regs[r] = content_opts::free{};
    assert_consistency();
    return r;
  }
  void clean_for_call(std::ostream &os, bool preserve_rdi, bool preserve_rsi) {
    assert_consistency();
    for (auto r : reg::volatiles) {
      if (r == rdi && preserve_rdi)continue;
      if (r == rsi && preserve_rsi)continue;
      auto orig = regs[r];
      std::visit(overloaded{
          [](content_opts::free) {},
          [&](content_opts::store_var sv) {
            auto r = lru.front_if(reg::is_non_volatile);
            if (content_opts::is_free(regs[r])) {

            } else {

            }

          },
          [](content_opts::saved_reg) {}
      }, regs[r]);
    }
    for (auto r : reg::volatiles) {
      if (r == rdi && preserve_rdi)continue;
      if (r == rsi && preserve_rsi)continue;
      assert(content_opts::is_free(regs[r]));
    }
    assert_consistency();
    THROW_WORK_IN_PROGRESS
  }
  register_t load_in_reg(var v, std::ostream &os) {
    assert_consistency();
    if (std::holds_alternative<var_loc::on_reg>(vars[v])) {
      //already in reg - but let's enhance its life
      register_t r = std::get<var_loc::on_reg>(vars[v]).r;
      lru.bring_back(r);
      assert_consistency();
      return r;
    }
    register_t r = free_reg(os);

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
        [&](const var_loc::on_stack &s) {
          //Move it
          os << "mov " << reg::to_string(r) << ", ";
          retrieve(v, os);
          os << " ; retrieving variable ";
          v.print(os);
          os << "\n";
          stack[s.p] = content_opts::free{};
          regs[r] = content_opts::store_var{.v=v};
          vars[v] = var_loc::on_reg{.r=r};
        },
        [&](const var_loc::on_reg &r) {
          //If it was already in reg, then we exited
          THROW_INTERNAL_ERROR
        },
    }, vars.at(v));

    assert_consistency();
    return r;
  }
  void ensure_no_both_mem(var v1, var v2, std::ostream &os) {
    if ((!std::holds_alternative<var_loc::on_stack>(vars[v1])) || (!std::holds_alternative<var_loc::on_stack>(vars[v2])))return; //one is already non-mem
    var_loc::on_stack s1 = std::get<var_loc::on_stack>(vars[v1]), s2 = std::get<var_loc::on_stack>(vars[v2]);
    load_in_reg(s1.p < s2.p ? v2 : v1, os); //load the most recent
  }

};
}
*/
namespace {

template<typename T>
struct __single_ignore {
  auto operator()(const T &) {}//TODO: might have to set void
};

template<typename ...Ts>
struct ignore : __single_ignore<Ts> ... { using __single_ignore<Ts>::operator()...; };

#define MATCH_INTERNAL_ERROR throw std::runtime_error( AT ": internal_error" );
//TODO: work out how to make the macro work

}

bool context_t::is_free(content_t c) {
  return std::holds_alternative<free>(c);
}
void context_t::assert_consistency() const {
  //regs
  for (auto r : reg::all) {
    std::visit(overloaded{
        [&](save r) {
          if (!reg::is_non_volatile(r)) {
            assert(reg::is_non_volatile(r));
          }
          if (r >= saved.size()) {
            assert(r < saved.size());
          }
          if (saved.at(r) != strict_location_t{r}) {
            assert(saved.at(r) == strict_location_t{r});
          }
        },
        [](free) {},
        [&](var v) {
          assert(vars.at(v) == location_t{r});
        },
    }, regs[r]);
  }
  //stack
  for (size_t i = 0; i < stack.size(); ++i) {
    std::visit(overloaded{
        [&](save r) {
          assert(saved.at(r) == strict_location_t{i});
        },
        [](free) {},
        [&](var v) {
          assert(vars.at(v) == location_t{on_stack{i}});
        },
    }, stack[i]);
  }
  //vars
  for (const auto &v_s : vars) {
    const var v = v_s.first;
    std::visit(overloaded{
        ignore<constant, global>(),
        [&](on_stack p) {
          assert(p < stack.size());
          assert(stack[p] == content_t{v});
        },
        [&](on_reg r) {
          assert(regs[r] == content_t{v});
        },
    }, v_s.second);
  }

  //saved
  for (auto sr : reg::non_volatiles) {
    std::visit(overloaded{
        [&](on_reg r) {
          assert (regs[r] == content_t{save{sr}});
        },
        [&](on_stack p) {
          assert(stack[p] == content_t{save{sr}});
        }
    }, saved[sr]);
  }
}
context_t::context_t() {
  for (auto r : reg::all)regs[r] = reg::is_volatile(r) ? content_t(free()) : content_t(save{r});
  for (auto r : reg::non_volatiles)saved[r] = on_reg{r};
  regs[rdi] = argv_var;
  vars[argv_var] = rdi;
  lru.bring_back(rdi);
  assert_consistency();
}
bool context_t::is_stack_empty() const {
  assert_consistency();
  return stack.empty();
}
bool context_t::are_nonvolatiles_restored() const {
  assert_consistency();
  for (auto r : reg::non_volatiles)if (saved[r] != strict_location_t{r})return false;
  return true;
}
bool context_t::are_volatiles_free(const std::vector<std::pair<var, register_t>> &except) const {
  assert_consistency();
  return std::all_of(reg::volatiles.begin(), reg::volatiles.end(), [&](register_t r) {
    if (auto it = std::find_if(except.begin(), except.end(), [r](const auto &vr) { return vr.second == r; }); it == except.end()) {
      return std::holds_alternative<free>(regs[r]);
    } else {
      return (std::holds_alternative<var>(regs[r]) && std::get<var>(regs[r]) == it->first);
    }
  });
}
bool context_t::are_volatiles_free(std::initializer_list<std::pair<var, register_t>> except) const {
  assert_consistency();
  return std::all_of(reg::volatiles.begin(), reg::volatiles.end(), [&](register_t r) {
    if (auto it = std::find_if(except.begin(), except.end(), [r](const auto &vr) { return vr.second == r; }); it == except.end()) {
      return std::holds_alternative<free>(regs[r]);
    } else {
      return (std::holds_alternative<var>(regs[r]) && std::get<var>(regs[r]) == it->first);
    }
  });
}
void context_t::retrieve(var v, std::ostream &os) const {
  assert_consistency();
  assert(vars.contains(v));
  std::visit(overloaded{
      [&](constant c) {
        os << c.value;
      },
      [&](const global &n) {
        os << n;
      },
      [&](on_stack p) {
        os << "qword [rsp";
        if (p < stack.size() - 1) {
          os << "+" << (stack.size() - 1 - p) * 8;
        }
        os << "]";
      },
      [&](on_reg r) {
        os << reg::to_string(r);
      },
  }, vars.at(v));
  assert_consistency();
}
std::string context_t::retrieve_to_string(var v) const {
  std::stringstream s;
  retrieve(v, s);
  return s.str();
}
context_t::streamable context_t::at(var v) const {
  return context_t::streamable(*this, v);
}
size_t context_t::stack_size() const {
  return stack.size();
}
bool context_t::contains(var v) const {
  return vars.contains(v);
}
void context_t::compress_stack(std::ostream &os) {
  assert_consistency();
  //TODO: maybe defragment
  size_t freed = 0;
  for (; !stack.empty() && std::holds_alternative<free>(stack.back()); stack.pop_back(), ++freed);
  if (freed)os << "add rsp, " << (freed * 8) << " ; reclaiming stack space\n";
  assert_consistency();
}
void context_t::destroy(const std::vector<var> &vs, std::ostream &os) {
  for (var v : vs) {
    destroy(v, os);
  }
}
void context_t::destroy(var v, std::ostream &os) {
  //TODO: maybe deep destruction
  os << "; destroying var " << v << " \n";
  assert_consistency();
  assert(vars.contains(v));
  std::visit(overloaded{
      ignore<constant, global>(),
      [&](on_stack p) {
        stack[p] = free{};
      },
      [&](on_reg r) {
        regs[r] = free{};
        lru.bring_front(r);
      },
  }, vars.at(v));
  vars.erase(v);
  compress_stack(os);
  assert_consistency();
}
void context_t::declare_const(var v, uint64_t value) {
  assert_consistency();
  assert(!vars.contains(v));
  vars[v] = constant{.value = value};
  assert_consistency();
}
void context_t::declare_global(var v, std::string_view name) {
  assert_consistency();
  assert(!vars.contains(v));
  vars[v] = global(name);
  assert_consistency();
}
void context_t::declare_move(var dst, var src) {
  assert_consistency();
  assert(!vars.contains(dst));
  assert(vars.contains(src));
  vars[dst] = std::move(vars[src]);
  vars.erase(src);
  std::visit(overloaded{ignore<global, constant>(),
                        [&](on_reg r) { regs[r] = dst; },
                        [&](on_stack p) { stack[p] = dst; }}, vars[dst]);
  assert_consistency();
}
void context_t::declare_copy(var dst, var src, std::ostream &os) {
  assert_consistency();
  assert(!vars.contains(dst));
  assert(vars.contains(src));
  std::visit(overloaded{
      [&](const std::variant<constant, global> &) { vars[dst] = vars[src]; },
      [&](const std::variant<on_reg, on_stack> &) {
        declare_free(dst, os);
        make_non_both_mem(src, dst, os);
        os << "mov " << at(dst) << ", " << at(src) << "\n";
      },
  }, vars[src]);
  assert_consistency();
}
void context_t::declare_free(var v, std::ostream &os) {
  assert_consistency();
  register_t r = free_reg(os);
  regs[r] = v;
  vars[v] = r;
  assert_consistency();
}
register_t context_t::free_reg(std::ostream &os) {
  assert_consistency();
  register_t r = lru.front();
  lru.bring_back(r);
  free_reg(r, os);
  assert_consistency();
  assert(std::holds_alternative<free>(regs[r]));
  return r;
}
void context_t::move_to_register(register_t src, register_t dst, std::ostream &os) {
  assert_consistency();
  assert(is_reg_free(dst));
  if (is_reg_free(src))return;
  std::visit(overloaded{
      ignore<free>(),
      [&](var v) {
        vars[v] = dst;
      },
      [&](save sr) {
        saved[sr] = dst;
      }
  }, regs[src]);
  std::swap(regs[dst], regs[src]);
  os << "mov " << reg::to_string(dst) << ", " << reg::to_string(src) << " \n";
  assert_consistency();
}
void context_t::move_to_stack(register_t r, std::ostream &os) {
  if (std::holds_alternative<free>(regs[r]))return;
  assert_consistency();
  size_t stack_id;
  if (auto it = std::find_if(stack.begin(), stack.end(), [](content_t c) { return std::holds_alternative<free>(c); });it == stack.end()) {
    stack_id = stack.size();
    os << "push " << reg::to_string(r) << "\n";
    stack.emplace_back(regs[r]);
  } else {
    stack_id = std::distance(stack.begin(), it);
    os << "mov qword [rsp+" << (stack.size() - stack_id - 1) * 8 << "], " << reg::to_string(r) << "\n";
    stack[stack_id] = regs[r];
  }
  regs[r] = free{};
  std::visit(overloaded{
      ignore<free>(),
      [&](save sr) { saved[sr] = on_stack{stack_id}; },
      [&](var v) { vars[v] = on_stack{stack_id}; }
  }, stack[stack_id]);
  assert_consistency();
}
void context_t::free_reg(register_t r, std::ostream &os) {
  assert_consistency();
  move_to_stack(r, os);
  assert_consistency();
}
void context_t::declare_in(var v, register_t r) {
  assert_consistency();
  if (!std::holds_alternative<free>(regs[r]))THROW_INTERNAL_ERROR;
  assert(!vars.contains(v));
  regs[r] = v;
  vars[v] = r;
  assert_consistency();
}
void context_t::make_non_mem(var v, std::ostream &os) {
  assert_consistency();
  assert(vars.contains(v));
  std::visit(overloaded{
      ignore<constant, global, on_reg>(),
      [&](on_stack p) {
        register_t r = free_reg(os);
        if (p == stack.size() - 1) {
          os << "pop " << reg::to_string(r) << "\n";
          stack.pop_back();
        } else {
          os << "mov " << reg::to_string(r) << ", " << at(v) << "\n";
          stack[p] = free{};
        }

        vars[v] = r;
        regs[r] = v;

      }
  }, vars.at(v));
  assert(!std::holds_alternative<on_stack>(vars[v]));
  assert_consistency();
}
void context_t::make_non_both_mem(var v1, var v2, std::ostream &os) {
  assert_consistency();
  if (is_mem(v1) && is_mem(v2)) {
    if (std::get<on_stack>(vars[v1]) < std::get<on_stack>(vars[v2]))std::swap(v1, v2);
    make_non_mem(v1, os);
  }
  assert(!is_mem(v1) || !is_mem(v2));
  assert_consistency();
}
bool context_t::is_mem(var v) const {
  assert(vars.contains(v));
  return std::holds_alternative<on_stack>(vars.at(v));
}
bool context_t::is_reg_free(register_t r) const {
  return std::holds_alternative<free>(regs.at(r));
}
void context_t::make_both_non_mem(var v1, var v2, std::ostream &os) {
  assert_consistency();
  make_non_mem(v1, os);
  make_non_mem(v2, os);
  assert_consistency();
  assert(!is_mem(v1) && !is_mem(v2));
}
//the args are MOVED. If you want to preserve them, don't pass them here but fill them later with declare_copy
void context_t::return_clean(const std::vector<std::pair<var, register_t>> &args, std::ostream &os) {
  assert_consistency();
  std::unordered_map<var, register_t> var_target;
  std::unordered_map<register_t, var> reg_target;
  for (const auto&[v, r] : args) {
    assert(vars.contains(v));
    assert(reg::is_volatile(r));
    assert(!var_target.contains(v));
    assert(!reg_target.contains(r));
    var_target[v] = r;
    reg_target[r] = v;
  }
  //assert(vars.size() == args.size()); // there shouldn't be more living variables //TODO: verify statement


  //Step 1. restore nonvolatiles
  for (auto r : reg::non_volatiles)
    if (regs[r] != content_t{r}) {
      std::visit(overloaded{
                     [&](on_reg sr) {
                       os << (is_reg_free(r) ? "mov " : "xchg ") << reg::to_string(r) << ", " << reg::to_string(sr) << "\n";
                       std::swap(regs[r], regs[sr]);
                       reassign(sr);
                       saved[r] = r;
                     },
                     [&](on_stack p) {
                       os << (is_reg_free(r) ? "mov " : "xchg ") << reg::to_string(r) << ", qword [rsp" << offset(stack.size() - 1 - p) << "]" << "\n";
                       std::swap(regs[r], stack[p]);
                       reassign(p);
                       saved[r] = r;
                     }
                 },
                 saved[r]);
    }
  assert_consistency();
  assert(are_nonvolatiles_restored());

  //Step 2. empty stack
  //Trick: if no holes, use all pops
  const bool use_pops = std::none_of(stack.begin(), stack.end(), is_free);
  for (size_t i = stack.size() - 1; ~i; --i) {
    std::visit(overloaded{
        [](free) {},
        [](save) { THROW_INTERNAL_ERROR },
        [&](var v) {
          assert(var_target.contains(v));
          register_t r = var_target.at(v);
          if (!is_reg_free(r)) {
            r = lru.front_volatile();
          }
          assert(is_reg_free(r));
          lru.bring_back(r);
          if (use_pops)os << "pop " << reg::to_string(r) << "\n";
          else os << "mov " << reg::to_string(r) << ", qword [rsp" << offset(stack_size() - 1 - i) << "]\n";
          stack[i] = free{};
          regs[r] = v;
          vars[v] = r;
        },
    }, stack[i]);
  }
  if (use_pops)stack.clear();
  compress_stack(os);

  assert_consistency();
  assert(are_nonvolatiles_restored());
  assert(stack.empty());

  //Step 4. volatile registers
  //4.1 set up real vars
  for (const auto&[v, r] : args) {
    if (vars[v] != location_t{r}) {
      std::visit(overloaded{
          ignore<constant, global>(),
          [&, r = r, v = v](on_reg sr) {
            if (is_reg_free(r)) {
              regs[r] = v;
              regs[sr] = free{};
              vars[v] = r;
              os << "mov " << reg::to_string(r) << ", " << reg::to_string(sr) << "\n";
            } else {
              var v2 = std::get<var>(regs[r]);
              std::swap(vars[v2], vars[v]);
              std::swap(regs[r], regs[sr]);
              os << "xchg " << reg::to_string(r) << ", " << reg::to_string(sr) << "\n";
            }
          },
          [](on_stack p) { THROW_INTERNAL_ERROR }
      }, vars[v]);
    }
  }

  //4.2 make concrete non-stored vars
  for (const auto&[v, r] : args) {
    if (vars[v] != location_t{r}) {
      assert(is_virtual(v));
      std::visit(overloaded{
          [&, v = v, r = r](constant c) {
            assert(is_reg_free(r));
            os << "mov " << reg::to_string(r) << ", " << c.value << "\n";
            regs[r] = v;
            vars[v] = r;
          },
          [&, v = v, r = r](const global &g) {
            assert(is_reg_free(r));
            os << "mov " << reg::to_string(r) << ", " << g << "\n";
            regs[r] = v;
            vars[v] = r;
          },
          [](on_reg sr) { THROW_INTERNAL_ERROR },
          [](on_stack p) { THROW_INTERNAL_ERROR }
      }, vars[v]);
    }
  }

  //Final assertions:
  assert_consistency();
  assert(stack.empty());
  assert(are_nonvolatiles_restored());
  assert(are_volatiles_free(args));
}
context_t context_t::merge(context_t c1, std::ostream &os1, context_t c2, std::ostream &os2) {
  THROW_WORK_IN_PROGRESS
}
//Args are MOVED. If you need copy, after calling perform the right copies.
void context_t::call_clean(const std::vector<std::pair<var, register_t>> &args, std::ostream &os) {
  assert_consistency();

  std::unordered_map<var, register_t> var_target;
  std::unordered_map<register_t, var> reg_target;
  for (const auto&[v, r] : args) {
    assert(vars.contains(v));
    assert(reg::is_volatile(r));
    assert(!var_target.contains(v));
    assert(!reg_target.contains(r));
    var_target[v] = r;
    reg_target[r] = v;
  }

  //move non virtual
  for (const auto&[v, r] : args)
    if (!is_virtual(v) && vars[v] != location_t{r}) {
      std::visit(overloaded{
          [](const constant &c) { THROW_INTERNAL_ERROR },
          [](const global &g) { THROW_INTERNAL_ERROR },
          [&, r = r, v = v](on_reg sr) {
            if (is_reg_free(r)) {
              std::swap(regs[r], regs[sr]);
              vars[v] = r;
              os << "mov " << reg::to_string(r) << ", " << reg::to_string(sr) << "\n";
            } else {
              var v2 = std::get<var>(regs[r]);
              std::swap(vars[v2], vars[v]);
              std::swap(regs[r], regs[sr]);
              os << "xchg " << reg::to_string(r) << ", " << reg::to_string(sr) << "\n";
            }
          },
          [&, r = r, v = v](on_stack p) {
            if (is_reg_free(r)) {
              regs[r] = v;
              stack[p] = free{};
              vars[v] = r;
              if (p == stack.size() - 1) {
                os << "pop " << reg::to_string(r) << "\n";
                stack.pop_back();
              } else {
                os << "mov " << reg::to_string(r) << ", qword [rsp" << offset(stack.size() - 1 - p) << "]\n";
              }
            } else {
              var v2 = std::get<var>(regs[r]);
              std::swap(vars[v2], vars[v]);
              std::swap(regs[r], stack[p]);
              os << "xchg " << reg::to_string(r) << ", qword [rsp" << offset(stack.size() - 1 - p) << "]\n";
            }
          }
      }, vars[v]);
    }

  //move registers
  for (register_t r : reg::volatiles)
    if (!is_reg_free(r) && !reg_target.contains(r)) {
      //try first registers
      auto nr = lru.front_non_volatile();
      if (is_reg_free(nr)) {
        //use reg
        move_to_register(r, nr, os);
        lru.bring_back(nr);
      } else {
        //use stack
        move_to_stack(r, os);
      }
    }

  //move virtuals
  for (const auto&[v, r] : args) {
    if (vars[v] != location_t{r}) {
      assert(is_virtual(v));
      std::visit(overloaded{
          [&, v = v, r = r](constant c) {
            assert(is_reg_free(r));
            os << "mov " << reg::to_string(r) << ", " << c.value << "\n";
            regs[r] = v;
            vars[v] = r;
          },
          [&, v = v, r = r](const global &g) {
            assert(is_reg_free(r));
            os << "mov " << reg::to_string(r) << ", " << g << "\n";
            regs[r] = v;
            vars[v] = r;
          },
          [](on_reg sr) { THROW_INTERNAL_ERROR },
          [](on_stack p) { THROW_INTERNAL_ERROR }
      }, vars[v]);
    }
  }

  assert_consistency();
  assert(are_volatiles_free(args));
}
void context_t::call_copy(const std::vector<std::pair<var, register_t>> &args, std::ostream &os) {
  for (const auto&[v, r] : args) {
    assert(is_reg_free(r));
    os << "mov " << reg::to_string(r) << ", " << at(v) << "\n";
    //TODO: increase refcount if necessary
  }
}
void context_t::reassign(context_t::strict_location_t l) {
  std::visit(overloaded{
      [&](on_reg r) {
        std::visit(overloaded{
            ignore<free>(),
            [&](var v) { vars[v] = r; },
            [&](save sr) { saved[sr] = r; }
        }, regs[r]);
      },
      [&](on_stack p) {
        std::visit(overloaded{
            ignore<free>(),
            [&](var v) { vars[v] = p; },
            [&](save sr) { saved[sr] = p; }
        }, stack[p]);
      }
  }, l);

}
bool context_t::is_virtual(var v) const {
  return std::visit(overloaded{
      [](const constant &) { return true; },
      [](const global &) { return true; },
      [](const on_reg &) { return false; },
      [](const on_stack &) { return false; },
  }, vars.at(v));
}

}