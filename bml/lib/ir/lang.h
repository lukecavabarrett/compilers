#ifndef COMPILERS_BML_LIB_IR_LANG_H_
#define COMPILERS_BML_LIB_IR_LANG_H_

#include <cstdint>
#include <variant>
#include <vector>
#include <string>
#include <optional>
namespace ir::lang {

struct memory_access;

struct var {

  const uint64_t id;
  constexpr explicit var(uint64_t id) : id(id) {}
  static inline uint64_t id_factory = 1;
  explicit var() : id(++id_factory) {}
  var(const var &) = default;
  var(var &&) = default;
  memory_access operator*() const;
  memory_access operator[](size_t idx) const;
};
constexpr var argv_var(0);

struct memory_access {
  var base;
  size_t block_offset;
};

struct operand {
  std::variant<var /* direct access*/ , memory_access> data;
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



};

#endif //COMPILERS_BML_LIB_IR_LANG_H_
