#include <bvm.h>
#include <cassert>
#include <charconv>
#include <algorithm>
#include <iostream>

namespace bvm {

namespace operand {

#define cc_reg(rname) word_t& reg_##rname ::get_lvalue(machine &m) {\
return m.r. rname ;\
}

cc_reg(rip);
cc_reg(rsp);
cc_reg(rax);
cc_reg(rbx);
cc_reg(rcx);
cc_reg(rdx);
cc_reg(rsi);
cc_reg(rdi);

word_t &mem_access_1::get_lvalue(machine &m) {
  word_t addr = base->get_rvalue(m);
  return m.m.deref(addr);
}
word_t &mem_access_2::get_lvalue(machine &m) {
  word_t addr = base->get_rvalue(m);
  addr += offset->get_rvalue(m);
  return m.m.deref(addr);
}
word_t &mem_access_3::get_lvalue(machine &m) {
  word_t addr = base->get_rvalue(m);
  addr += offset->get_rvalue(m) * step->get_rvalue(m);
  return m.m.deref(addr);
}
}

namespace instruction {
std::optional<word_t> jmp::execute_on(machine &m) {
  m.r.rip = where->get_rvalue(m) - 1; //because the machine will increment $rip anyway
  return {};
}

std::optional<word_t> jmpz::execute_on(machine &m) {
  if (what->get_rvalue(m) == 0)m.r.rip = where->get_rvalue(m) - 1; //because the machine will increment $rip anyway
  return {};
}

std::optional<word_t> jmpnz::execute_on(machine &m) {
  if (what->get_rvalue(m) != 0)m.r.rip = where->get_rvalue(m) - 1; //because the machine will increment $rip anyway
  return {};
}

std::optional<word_t> jmpl::execute_on(machine &m) {
  if (w1->get_rvalue(m) < w2->get_rvalue(m))m.r.rip = where->get_rvalue(m) - 1; //because the machine will increment $rip anyway
  return {};
}

std::optional<word_t> jmple::execute_on(machine &m) {
  if (w1->get_rvalue(m) <= w2->get_rvalue(m))m.r.rip = where->get_rvalue(m) - 1; //because the machine will increment $rip anyway
  return {};
}

std::optional<word_t> print_int::execute_on(machine &m) {
  printf("%ld\n", int64_t(what->get_rvalue(m)));
  return {};
}

std::optional<word_t> scan_int::execute_on(machine &m) {
  scanf("%ld", &(what->get_lvalue(m)));
  return {};
}

std::optional<word_t> push::execute_on(machine &m) {
  m.m.s.push(what->get_rvalue(m));
  m.r.rsp++;
  return {};
}

std::optional<word_t> push_p::execute_on(machine &m) {
  m.m.s.push(what->get_rvalue(m) + offset->get_rvalue(m));
  m.r.rsp++;
  return {};
}

std::optional<word_t> pop::execute_on(machine &m) {
  what->get_lvalue(m) = m.m.s.pop();
  m.r.rsp--;
  return {};
}

}

}