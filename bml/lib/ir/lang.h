#ifndef COMPILERS_BML_LIB_IR_LANG_H_
#define COMPILERS_BML_LIB_IR_LANG_H_

#include <util/util.h>
#include <cstdint>
#include <variant>
#include <vector>
#include <string>
#include <optional>
#include <memory>
#include <util/texp.h>

namespace ir::lang {

struct var;
struct ternary;
namespace instruction {
namespace rhs_expr {
struct constant;
struct global;
struct copy;
struct memory_access;
struct malloc;
struct apply_fn;
typedef std::unique_ptr<ternary> branch;
typedef std::variant<constant, global, copy, memory_access, malloc, apply_fn, branch> t;
}
struct assign;
struct write_uninitialized_mem;
struct cmp_vars;
typedef std::variant<assign, write_uninitialized_mem, cmp_vars> t;
}
namespace rhs_expr = instruction::rhs_expr;

struct var {
  uint64_t id;
  constexpr explicit var(uint64_t id) : id(id) {}
  static inline uint64_t id_factory = 1;
  explicit var() : id(++id_factory) {}
  var(const var &) = default;
  var(var &&) = default;
  var &operator=(const var &) = default;
  var &operator=(var &&) = default;
  operator rhs_expr::copy() const;
  rhs_expr::memory_access operator*() const;
  rhs_expr::memory_access operator[](size_t idx) const;
  instruction::assign assign(rhs_expr::t &&) const;
  bool operator==(const var &v) const { return id == v.id; }
  bool operator!=(const var &v) const { return id != v.id; }
  void print(std::ostream &os) const { if (id)os << "var_" << id; else os << "var_argv"; }
};
constexpr var argv_var(0);
namespace instruction {
namespace rhs_expr {
//src
struct constant {
  const uint64_t v;
  constant(uint64_t v) : v(v) {}
};
struct global { std::string name; };
struct copy { var v; };
struct memory_access {
  var base;
  size_t block_offset;
};
struct malloc { size_t size; };
struct apply_fn { var f, x; };
//TODO: add unary_op, binary_op
}
struct assign {
  var dst;
  rhs_expr::t src;
};
struct write_uninitialized_mem {
  var base;
  size_t block_offset;
  var src;
};
struct cmp_vars {
  var v1, v2;
  enum cmp_ops { test, cmp }; //TODO: add others
  static std::string_view ops_to_string(cmp_ops op){
    switch (op) {

      case test:return "test";
      case cmp:return "cmp";
      default:
      THROW_UNIMPLEMENTED;
    }
  }
  cmp_ops op;
};
};

struct scope {
  std::vector<instruction::t> body;
  std::unordered_map<std::size_t, std::vector<var> > destroys; // destroys[i] must be destroyed before instruction i
  void push_back(instruction::t &&i);
  scope &operator<<(instruction::t &&i);
  var ret;
  void compile_as_function(std::ostream &os);
  void print(std::ostream &os, size_t offset = 0);
};

struct ternary {
  enum jmp_instr { jmp, jne, jle, jz }; //TODO: add others
  jmp_instr cond;
  scope nojmp_branch, jmp_branch;
  std::string_view ops_to_string(){
    switch (cond) {

      case jmp:return "jmp";
      case jne:return "jne";

      case jle:return "jle";
      case jz:return "jz";
      default:
        THROW_UNIMPLEMENTED
    }
  }
};


/*
struct operand {
  std::variant<var , memory_access> data;
  operand(const operand &) = default;
  operand(operand &&) = default;
  operand(const var &v) : data(v) {}
  operand(const memory_access &m) : data(m) {}
};

struct operands {
  operand op1;
  std::optional<operand> op2;
};

enum class opcodes {
  mov, add, cmp, ret, sar
};

struct instruction {
  opcodes opcode;
  operands ops;
};

#define INSTR_OPS_2(mnem) instruction mnem (operand o1, operand o2){ return instruction{.opcode=opcodes:: mnem , .ops={.op1 = o1, .op2 = o2}}; }
#define INSTR_OPS_H_2(mnem) instruction mnem (operand o1, operand o2);
#define INSTR_OPS_1(mnem) instruction mnem (operand o1){ return instruction{.opcode=opcodes:: mnem , .ops={.op1 = o1, .op2 = {}}}; }
#define INSTR_OPS_H_1(mnem) instruction mnem (operand o1);

INSTR_OPS_H_2 (mov)
INSTR_OPS_H_2 (add)
INSTR_OPS_H_2 (cmp)
INSTR_OPS_H_1 (sar)
INSTR_OPS_H_1 (ret)

struct ternary;
struct scope {
  std::vector<std::variant<ternary, instruction>> ops;
  void push_back(instruction&& i);
  void push_back(ternary&& t);
  scope& operator<<(instruction&& i);
  scope& operator<<(ternary&& i);
};
struct ternary {
  std::string jmp_if_false;
  scope true_branch, false_branch;
};

struct function : public scope {

};

*/

};

#endif //COMPILERS_BML_LIB_IR_LANG_H_
