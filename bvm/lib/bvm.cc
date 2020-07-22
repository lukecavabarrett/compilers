#include <bvm.h>
#include <cassert>
#include <charconv>
#include <algorithm>
#include <iostream>

namespace bvm {

std::string_view trim(std::string_view s) {
  while (!s.empty() && s.front() <= 32)s.remove_prefix(1);
  while (!s.empty() && s.back() <= 32)s.remove_suffix(1);
  return s;
}

namespace operand {

#define cc_reg(rname) word_t& reg_##rname ::get_lvalue(machine &m) {\
return m.r. rname ;\
}\

cc_reg(rip);
cc_reg(rax);
cc_reg(rbx);
cc_reg(rcx);
cc_reg(rdx);
cc_reg(rsi);
cc_reg(rdi);

#define PARSE_DEBUG 0

std::unique_ptr<t> t::parse(std::string_view s) {
  s = trim(s);
#if PARSE_DEBUG
  std::cerr << "Parsing operand \"" << s << "\"" << std::endl;
#endif

  if (s.empty())throw "parse";
  switch (s.front()) {
    case '$' : return immediate::parse(s);
    case '%' : return reg::parse(s);
    case '(' : return mem_access::parse(s);
  }
  assert(false);
}
std::unique_ptr<immediate> immediate::parse(std::string_view s) {
  assert(s.front() == '$');
  s.remove_prefix(1);
  assert(!s.empty());
  int base = 10;
  if (s.front() == '0') {
    base = 8;
    s.remove_prefix(1);
  }
  if (s.empty())return std::make_unique<immediate>(0);
  if (s.front() == 'x' || s.front() == 'X') {
    base = 16;
    s.remove_prefix(1);
  }
  assert(!s.empty());
  int64_t n;
  auto[p, ec] = std::from_chars(s.begin(), s.end(), n, base);
  if (ec != std::errc()) {
    std::cerr << s << " is not a valid literal" << std::endl;
    assert(false);
  }
  return std::make_unique<immediate>(n);
}
std::unique_ptr<reg> reg::parse(std::string_view s) {
  assert(s.front() == '%');
  s.remove_prefix(1);

#define case(r) if(s==#r)return std::make_unique<reg_##r >();
  case(rip);
  case(rax);
  case(rbx);
  case(rcx);
  case(rdx);
  case(rsi);
  case(rdi);
#undef case
  std::cerr << "error: \"" << s << "\" is not a valid register" << std::endl;
  assert(false);
}
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
std::unique_ptr<mem_access> mem_access::parse(std::string_view s) {
  assert(s.front() == '(');
  assert(s.back() == ')');
  s.remove_prefix(1);
  s.remove_suffix(1);
  assert(std::find(s.begin(), s.end(), '(') == s.end());
  assert(std::find(s.begin(), s.end(), ')') == s.end());
  auto first_comma = std::find(s.begin(), s.end(), ',');
  auto base = t::parse(std::string_view(s.begin(), std::distance(s.begin(), first_comma)));
  if (first_comma == s.end()) {
    //simple access
    return std::make_unique<mem_access_1>(std::move(base));
  }
  ++first_comma;
  auto second_comma = std::find(first_comma, s.end(), ',');
  auto offset = t::parse(std::string_view(first_comma, std::distance(first_comma, second_comma)));
  if (second_comma == s.end()) {
    //offset access
    return std::make_unique<mem_access_2>(std::move(base), std::move(offset));
  }
  ++second_comma;
  auto end = std::find(second_comma, s.end(), ',');
  assert(end == s.end());
  auto step = t::parse(std::string_view(second_comma, std::distance(second_comma, end)));
  //offset+step access
  return std::make_unique<mem_access_3>(std::move(base), std::move(offset), std::move(step));
}
std::unique_ptr<t> parse(std::string_view s) { return t::parse(s); }
}

namespace instruction {
std::optional<word_t> jmp::execute_on(machine &m) {
  m.r.rip = where->get_rvalue(m) - 1; //because the machine will increment $rip anyway
  return {};
}

std::optional<word_t> print_int::execute_on(machine &m) {
  printf("%lld\n",int64_t(what->get_rvalue(m)));
  return {};
}

std::optional<word_t> scan_int::execute_on(machine &m) {
  scanf("%lld",&(what->get_lvalue(m)));
  return {};
}

template<typename InputIt, typename T>
InputIt find_nested(InputIt first, InputIt last, const T &value) {
  size_t n = 0;
  for (; first != last; ++first) {
    if (n == 0 && *first == value)return first;
    if (*first == '(')++n;
    if (*first == ')')--n;
  }
  return first;
}

std::unique_ptr<operand::lvalue> to_lvalue(std::unique_ptr<operand::t> &&o) {
  auto *p = dynamic_cast<operand::lvalue *>(o.release());
  assert(p);
  return std::unique_ptr<operand::lvalue>(p);
}

std::unique_ptr<operand::rvalue> to_rvalue(std::unique_ptr<operand::t> &&o) {

  auto *p = dynamic_cast<operand::rvalue *>(o.release());
  assert(p);
  return std::unique_ptr<operand::rvalue>(p);
}

std::unique_ptr<t> parse(std::string_view s) {
  s = trim(s);
  std::string_view o(s.begin(), std::distance(s.begin(), std::find_if(s.begin(), s.end(), [](char p) { return p <= 32; })));
  s.remove_prefix(o.size());
  s = trim(s);
  std::vector<std::unique_ptr<operand::t>> ops;
  while (!s.empty()) {
    std::string_view o(s.begin(), std::distance(s.begin(), find_nested(s.begin(), s.end(), ',')));
    s.remove_prefix(o.size());
    if (!s.empty())s.remove_prefix(1);
    s = trim(s);
    ops.push_back(operand::parse(o));
  }
  if (ops.size() == 0) {
  } else if (ops.size() == 1) {
    if (o == "exit") return std::make_unique<exit>(to_rvalue(std::move(ops[0])));
    if (o == "jmp") return std::make_unique<jmp>(to_rvalue(std::move(ops[0])));
    if (o == "print_int") return std::make_unique<print_int>(to_rvalue(std::move(ops[0])));
    if (o == "scan_int") return std::make_unique<scan_int>(to_lvalue(std::move(ops[0])));
  } else if (ops.size() == 2) {
    if (o == "add")
      return std::make_unique<add2>(
          to_lvalue(std::move(ops[0])),
          to_rvalue(std::move(ops[1])));
    if (o == "sub")
      return std::make_unique<sub2>(
          to_lvalue(std::move(ops[0])),
          to_rvalue(std::move(ops[1])));
    if (o == "mul")
      return std::make_unique<mul2>(
          to_lvalue(std::move(ops[0])),
          to_rvalue(std::move(ops[1])));
    if (o == "div")
      return std::make_unique<div2>(
          to_lvalue(std::move(ops[0])),
          to_rvalue(std::move(ops[1])));
  }
  else if (ops.size() == 3) {

    if (o == "add")
      return std::make_unique<add3>(
          to_lvalue(std::move(ops[0])),
          to_rvalue(std::move(ops[1])),
          to_rvalue(std::move(ops[2])));
  }
  std::cerr << "unrecognized instruction \"" << o << "\" with " << ops.size() << " operands. " << std::endl;
  assert(false);
}

}

}