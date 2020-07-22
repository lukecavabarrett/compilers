#ifndef COMPILERS_BVM_LIB_BVM_H_
#define COMPILERS_BVM_LIB_BVM_H_

#include <cstdint>
#include <vector>
#include <memory>
#include <unordered_map>
namespace bvm {

typedef uint64_t word_t;

/** The first 32 bit of the word indentifies the page, the second 32 the offset
 * page 0 -> code
 * page 1 -> stack
 * other pages -> heap, or rodata, or data
 * */

struct registers {
  word_t rip = 0; // instruction pointer
  word_t rax = 0, rbx = 0, rcx = 0, rdx = 0, rsi = 0, rdi = 0; // classic "general_purpose" registers
};

struct stack {
  std::vector<word_t> data;
  void make_space(uint64_t s) {
    data.resize(data.size() + s);
  }
  void push(word_t w) { data.push_back(w); }
  word_t pop() {
    word_t w = data.back();
    data.pop_back();
    return w;
  }
  word_t top() { return data.back(); }
  word_t &top(size_t s) {
    return data.at(data.size() - s - 1);
  }

};

struct heap {
  struct page {
    uint32_t id, size;
    std::unique_ptr<word_t[]> data;
  };
  std::unordered_map<uint32_t, page> pages;
  uint32_t next_page = 100;
  uint32_t create_page(uint32_t s) {
    ++next_page;
    pages[next_page] = page{.id=next_page, .size=s, .data=std::make_unique<word_t[]>(s)};
    return next_page;
  }
  void free_page(uint32_t s) {
    pages.erase(s);
  }
};

struct memory {
  word_t &deref(word_t addr) {
    uint32_t hi = addr >> 32;
    uint32_t lo = addr;
    if (hi == 0) {
      throw; //cannot dereference code
    } else if (hi == 1) {
      return s.data[lo];
    } else {
      return h.pages.at(hi).data[lo];
    }
  }
  stack s;
  heap h;
};

class machine;
namespace operand {

struct t {
  virtual word_t get_rvalue(machine &m) = 0;
  virtual ~t() = default;
  static std::unique_ptr<t> parse(std::string_view s);
};
std::unique_ptr<t> parse(std::string_view s);
struct rvalue : public t {
};

struct lvalue : public rvalue {
  virtual word_t &get_lvalue(machine &m) = 0;
  virtual word_t get_rvalue(machine &m) { return get_lvalue(m); }
};

struct immediate : public rvalue {
  word_t value;
  immediate(word_t w) : value(w) {}
  virtual word_t get_rvalue(machine &m) final { return value; }
  static std::unique_ptr<immediate> parse(std::string_view s);

};

struct reg : public lvalue {
  static std::unique_ptr<reg> parse(std::string_view s);
};

#define header_reg(r) struct reg_##r : public reg {\
virtual word_t& get_lvalue(machine &m) final;\
};

header_reg(rip);
header_reg(rax);
header_reg(rbx);
header_reg(rcx);
header_reg(rdx);
header_reg(rsi);
header_reg(rdi);

struct mem_access : public lvalue {
  static std::unique_ptr<mem_access> parse(std::string_view s);
};

struct mem_access_1 : public mem_access {
  std::unique_ptr<t> base;
  mem_access_1(std::unique_ptr<t> &&b) : base(std::move(b)) {}
  virtual word_t &get_lvalue(machine &m) final;
};

struct mem_access_2 : public mem_access {
  std::unique_ptr<t> base, offset;
  mem_access_2(std::unique_ptr<t> &&b, std::unique_ptr<t> &&o) : base(std::move(b)), offset(std::move(o)) {}

  virtual word_t &get_lvalue(machine &m) final;
};

struct mem_access_3 : public mem_access {
  std::unique_ptr<t> base, offset, step;
  mem_access_3(std::unique_ptr<t> &&b, std::unique_ptr<t> &&o, std::unique_ptr<t> &&s) : base(std::move(b)), offset(std::move(o)), step(std::move(s)) {}

  virtual word_t &get_lvalue(machine &m) final;
};

}

namespace instruction {

struct t {
  virtual std::optional<word_t> execute_on(machine &m) = 0;
  virtual ~t() = default;
};

std::unique_ptr<t> parse(std::string_view s);

struct exit : public t {
  std::unique_ptr<operand::rvalue> what;
  exit(std::unique_ptr<operand::rvalue> &&w) : what(std::move(w)) {};
  virtual std::optional<word_t> execute_on(machine &m) final {
    return what->get_rvalue(m);
  }
};

struct jmp : public t {
  std::unique_ptr<operand::rvalue> where;
  jmp(std::unique_ptr<operand::rvalue> &&w) : where(std::move(w)) {};

  virtual std::optional<word_t> execute_on(machine &m) final;
};

struct print_int : public t {
  std::unique_ptr<operand::rvalue> what;
  print_int(std::unique_ptr<operand::rvalue> &&w) : what(std::move(w)) {};

  virtual std::optional<word_t> execute_on(machine &m) final;
};

struct scan_int : public t {
  std::unique_ptr<operand::lvalue> what;
  scan_int(std::unique_ptr<operand::lvalue> &&w) : what(std::move(w)) {};

  virtual std::optional<word_t> execute_on(machine &m) final;
};

template<typename Op>
struct binary_op : public t {
  virtual std::optional<word_t> execute_on(machine &m) final {
    lhs->get_lvalue(m) = Op()(rhs_1->get_rvalue(m), rhs_2->get_rvalue(m));
    return {};
  }
  binary_op(std::unique_ptr<operand::lvalue> &&l, std::unique_ptr<operand::rvalue> &&r1, std::unique_ptr<operand::rvalue> &&r2)
      : lhs(std::move(l)), rhs_1(std::move(r1)), rhs_2(std::move(r2)) {}
  std::unique_ptr<operand::lvalue> lhs;
  std::unique_ptr<operand::rvalue> rhs_1, rhs_2;
};

typedef binary_op<std::plus<word_t>> add3;
typedef binary_op<std::minus<word_t>> sub3;
typedef binary_op<std::multiplies<word_t>> mul3;
typedef binary_op<std::divides<word_t>> div3;

template<typename Op>
struct binary_op_fold : public t {
  virtual std::optional<word_t> execute_on(machine &m) final {
    lhs->get_lvalue(m) = Op()(lhs->get_rvalue(m), rhs->get_rvalue(m));
    return {};
  }
  binary_op_fold(std::unique_ptr<operand::lvalue> &&l, std::unique_ptr<operand::rvalue> &&r)
      : lhs(std::move(l)), rhs(std::move(r)) {}
  std::unique_ptr<operand::lvalue> lhs;
  std::unique_ptr<operand::rvalue> rhs;
};

typedef binary_op_fold<std::plus<word_t>> add2;
typedef binary_op_fold<std::minus<word_t>> sub2;
typedef binary_op_fold<std::multiplies<word_t>> mul2;
typedef binary_op_fold<std::divides<word_t>> div2;

}

struct machine {

  registers r;
  memory m;
};

}

#endif //COMPILERS_BVM_LIB_BVM_H_
