#include <parse_asm.h>
#include <cassert>
#include <charconv>
#include <iostream>
#include <algorithm>

namespace bvm {

namespace {
std::string_view trim(std::string_view s) {
  while (!s.empty() && s.front() <= 32)s.remove_prefix(1);
  while (!s.empty() && s.back() <= 32)s.remove_suffix(1);
  return s;
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

}

namespace operand {

std::unique_ptr<t> asm_parse(std::string_view s) {
  s = trim(s);
  if (s.empty())throw "parse";
  switch (s.front()) {
    case '$' : return asm_parse_immediate(s);
    case '%' : return asm_parse_reg(s);
    case '(' : return asm_parse_mem_access(s);
  }
  std::cerr << " labels are not allowed here " << std::endl;
  assert(false);
}

std::unique_ptr<t> asm_parse(std::string_view s, const std::unordered_map<std::string, word_t> &l) {
  s = trim(s);
  if (s.empty())throw "parse";
  if (s.front() == '$' || s.front() == '%' || s.front() == '(')return asm_parse(s);
  if (l.find(std::string(s)) == l.end()) {
    std::cerr << " label \"" << s << "\" was never defined" << std::endl;
    assert(false);
  }
  return std::make_unique<immediate>(l.at(std::string(s)));

}

std::unique_ptr<reg> asm_parse_reg(std::string_view s) {
  assert(s.front() == '%');
  s.remove_prefix(1);

#define case(r) if(s==#r)return std::make_unique<reg_##r >();
  case(rip);
  case(rsp);
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

std::unique_ptr<immediate> asm_parse_immediate(std::string_view s) {
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

std::unique_ptr<mem_access> asm_parse_mem_access(std::string_view s) {
  assert(s.front() == '(');
  assert(s.back() == ')');
  s.remove_prefix(1);
  s.remove_suffix(1);
  assert(std::find(s.begin(), s.end(), '(') == s.end());
  assert(std::find(s.begin(), s.end(), ')') == s.end());
  auto first_comma = std::find(s.begin(), s.end(), ',');
  auto base = asm_parse(std::string_view(s.begin(), std::distance(s.begin(), first_comma)));
  if (first_comma == s.end()) {
    //simple access
    return std::make_unique<mem_access_1>(std::move(base));
  }
  ++first_comma;
  auto second_comma = std::find(first_comma, s.end(), ',');
  auto offset = asm_parse(std::string_view(first_comma, std::distance(first_comma, second_comma)));
  if (second_comma == s.end()) {
    //offset access
    return std::make_unique<mem_access_2>(std::move(base), std::move(offset));
  }
  ++second_comma;
  auto end = std::find(second_comma, s.end(), ',');
  assert(end == s.end());
  auto step = asm_parse(std::string_view(second_comma, std::distance(second_comma, end)));
  //offset+step access
  return std::make_unique<mem_access_3>(std::move(base), std::move(offset), std::move(step));
}

}

namespace instruction {

std::unique_ptr<t> asm_parse(std::string_view s, const std::unordered_map<std::string, word_t> &l) {
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
    ops.push_back(operand::asm_parse(o, l));
  }
  if (ops.size() == 0) {
#define make_op(iname, oname) if (o== #iname ) return std::make_unique< oname >();
    make_op(nop, nop);
#undef make_op
  } else if (ops.size() == 1) {
#define make_op(iname, oname, v1) if (o== #iname ) return std::make_unique< oname >( to_##v1 (std::move(ops[0])));
    make_op(exit, exit, rvalue);
    make_op(jmp, jmp, rvalue);
    make_op(print_int, print_int, rvalue);
    make_op(scan_int, scan_int, lvalue);
    make_op(push, push, rvalue);
    make_op(pop, pop, lvalue);
 #undef make_op
  } else if (ops.size() == 2) {

#define make_op(iname, oname, v1, v2) if (o== #iname ) return std::make_unique< oname >( to_##v1 (std::move(ops[0])), to_##v2 (std::move(ops[1])));
    make_op(add, add2, lvalue, rvalue);
    make_op(sub, sub2, lvalue, rvalue);
    make_op(mul, mul2, lvalue, rvalue);
    make_op(div, div2, lvalue, rvalue);
    make_op(mov, mov2, lvalue, rvalue);
    make_op(jmpz, jmpz, rvalue, rvalue);
    make_op(jmpnz, jmpnz, rvalue, rvalue);
    make_op(push_add, push_p, rvalue,rvalue);
#undef make_op
  } else if (ops.size() == 3) {

#define make_op(iname, oname, v1, v2, v3) if (o== #iname ) return std::make_unique< oname >( to_##v1 (std::move(ops[0])), to_##v2 (std::move(ops[1])), to_##v3 (std::move(ops[2])));

    make_op(add, add3, lvalue, rvalue, rvalue);
    make_op(jmpl, jmpl, rvalue, rvalue, rvalue);
    make_op(jmple, jmple, rvalue, rvalue, rvalue);
#undef make_op
  }
  std::cerr << "unrecognized instruction \"" << o << "\" with " << ops.size() << " operands. " << std::endl;
  assert(false);
}

}

std::vector<std::unique_ptr<instruction::t> > asm_parse_file(std::string_view s) {
  std::unordered_map<std::string, word_t> labels;
  std::vector<std::string_view> lines;

  //lines pass
  for (auto begin = s.begin(), end = std::find(begin, s.end(), 10); begin != s.end(); begin = end + (*end == 10), end = std::find(begin, s.end(), 10)) {
    if (begin == end)continue;
    std::string_view s(begin, end - begin);
    s = trim(s);
    if (s.empty())continue;
    for (auto it = std::find(s.begin(), s.end(), ':'); !s.empty() && s.front() != '#' && it != s.end();) {
      //new label
      std::string label(s.begin(), it - s.begin());
      assert(labels.find(label) == labels.end()); // repeated label!
      labels[label] = lines.size();
      s.remove_prefix(label.size() + 1);
      s = trim(s);
    }
    if (s.empty() || s.front() == '#')continue;//comment
    s.remove_suffix(s.end() - std::find(s.begin(), s.end(), '#'));
    lines.push_back(s);
  }

  //line parse pass
  std::vector<std::unique_ptr<instruction::t> > instructions;
  instructions.reserve(lines.size());
  for (std::string_view s : lines)instructions.emplace_back(instruction::asm_parse(s, labels));
  return instructions;
}
}