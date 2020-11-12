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
struct unary_op;
struct binary_op;
typedef std::variant<constant, global, copy, memory_access, malloc, apply_fn, branch, unary_op, binary_op> t;
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
  explicit var(std::string_view name) : id(++id_factory) {maybe_names.try_emplace(id,name);}
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
  void print(std::ostream &os) const {
    if(maybe_names.contains(id))os<<maybe_names.at(id)<<"_"<<id;
    else os << "var__" << id;
  }
  static std::unordered_map<uint64_t,std::string> maybe_names;
};
constexpr var argv_var(0);
namespace instruction {
namespace rhs_expr {
//src
struct constant {//Trivially destructible
  const uint64_t v;
  constant(uint64_t v) : v(v) {}
};
struct global { std::string name; };// trivially destructible
struct copy { var v; }; //TODO: remove
struct memory_access { //TODO: mark destruction_class
  var base;
  size_t block_offset;
};
struct malloc { size_t size; }; //TODO: mark destruction class
struct apply_fn { var f, x; }; //TODO: mark destruction class (might be maybe_non_trivial)
struct binary_op { //Assert inputs are trivial; result should be trivial
  enum ops { add, sub };
  static std::string_view ops_to_string(ops op) {
    switch (op) {
      case add:return "add";
      case sub:return "sub";
      default:THROW_UNIMPLEMENTED;
    }
  }
  static bool is_commutative(ops op){
    switch (op) {
      case add:return true;
      case sub:return false;
      default:THROW_UNIMPLEMENTED;
    }
  }
  ops op;
  var x1, x2;
};

struct unary_op { //assert input is trivial; result should be trivial
  enum ops { sal, sar };
  static std::string_view ops_to_string(ops op) {
    switch (op) {
      case sal:return "sal";
      case sar:return "sar";
      default:THROW_UNIMPLEMENTED;
    }
  }
  ops op;
  var x;
};
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
  enum ops { test, cmp }; //TODO: add others
  static std::string_view ops_to_string(ops op) {
    switch (op) {

      case test:return "test";
      case cmp:return "cmp";
      default:THROW_UNIMPLEMENTED;
    }
  }
  ops op;
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
  std::string_view ops_to_string() {
    switch (cond) {

      case jmp:return "jmp";
      case jne:return "jne";

      case jle:return "jle";
      case jz:return "jz";
      default:THROW_UNIMPLEMENTED
    }
  }
};

};

#endif //COMPILERS_BML_LIB_IR_LANG_H_
