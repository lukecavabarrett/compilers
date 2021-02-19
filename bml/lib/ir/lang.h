#ifndef COMPILERS_BML_LIB_IR_LANG_H_
#define COMPILERS_BML_LIB_IR_LANG_H_

#include <util/util.h>
#include <cassert>
#include <cstdint>
#include <variant>
#include <vector>
#include <string>
#include <optional>
#include <memory>
#include <util/message.h>

namespace ir::lang {
namespace parse {
class tokenizer;
}
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

/*
 TODO: destroy tag
 then inference algorithm
}*/
enum destroy_class_t : uint8_t {
  unvalid_destroy_class = 0,
  unboxed = 1,
  global = 2,
  non_trivial = 4,
  value = unboxed | global | non_trivial,
  trivial = unboxed | global,
  boxed = global | non_trivial,
  non_global = value & (~global)
};
static_assert(unvalid_destroy_class == 0);
static_assert(trivial == 3);
static_assert(boxed == 6);
static_assert(non_global == 5);
static_assert(value == 7);
namespace parse {
std::optional<destroy_class_t> destroy_class_of_string(std::string_view str);
}
std::string_view destroy_class_to_string(destroy_class_t);
destroy_class_t operator|(const destroy_class_t a, const destroy_class_t b);
destroy_class_t operator&(const destroy_class_t a, const destroy_class_t b);
bool operator<=(const destroy_class_t a, const destroy_class_t b);
bool operator>=(const destroy_class_t a, const destroy_class_t b);
bool operator<(const destroy_class_t a, const destroy_class_t b);
bool operator>(const destroy_class_t a, const destroy_class_t b);
struct var {

  uint64_t id;
  //constexpr explicit var(uint64_t id) : id(id) {}
  static inline uint64_t id_factory = 0;
  explicit var() : id(id_factory++) {
    destroy_classes.push_back(value);
    assert(destroy_classes.size() == id + 1);
  }
  explicit var(std::string_view name) : var() {
    maybe_names.try_emplace(id, name);
    ++(name_size[name]);
  }
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
    if (maybe_names.contains(id))os << maybe_names.at(id) << "_" << id;
    else os << "var__" << id;
  }
  [[nodiscard]] std::string_view name() const {
    return maybe_names.contains(id) ? std::string_view(maybe_names.at(id)) : "_nan_";
  }
  static std::unordered_map<uint64_t, std::string> maybe_names;
  static std::unordered_map<std::string_view, size_t> name_size;
  static std::vector<destroy_class_t> destroy_classes;
  destroy_class_t &destroy_class() { return destroy_classes.at(id); };
  const destroy_class_t &destroy_class() const { return destroy_classes.at(id); };
  var &mark(destroy_class_t d) {
    destroy_class() = d;
    return *this;
  }
};
std::ostream &operator<<(std::ostream &os, const var &v);
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
  enum ops { add, sub, sal, sar, mul, div, imul, idiv };
  static std::string_view ops_to_string(ops op) {
    switch (op) {
      case add:return "add";
      case sub:return "sub";
      case sal:return "sal";
      case sar:return "sar";
      case mul:return "mul";
      case div:return "div";
      case imul:return "imul";
      case idiv:return "idiv";
      default:THROW_UNIMPLEMENTED;
    }
  }
  static ops string_to_op(std::string_view s) {
    if (s == "add")return add;
    if (s == "sub")return sub;
    if (s == "sal")return sal;
    if (s == "sar")return sar;
    if (s == "mul")return mul;
    if (s == "div")return div;
    if (s == "imul")return imul;
    if (s == "idiv")return idiv;
    THROW_UNIMPLEMENTED
  }
  static bool is_commutative(ops op) {
    switch (op) {
      case mul:
      case imul:
      case add:return true;
      case sal:
      case sar:
      case sub:
      case div:
      case idiv:return false;
      default:THROW_UNIMPLEMENTED;
    }
  }
  uint64_t of_constant(uint64_t a, uint64_t b) {
    switch (op) {
      case mul: return a * b;
      case imul: return int64_t(a) * int64_t(b);
      case add: return a + b;
      case sal: return int64_t(a) << b;
      case sar:return int64_t(a) >> b;
      case sub:return a - b;
      case div:return a / b;
      case idiv:return int64_t(a) / int64_t(b);
      default:THROW_UNIMPLEMENTED;
    }
  }
  ops op;
  var x1, x2;
};

struct unary_op { //assert input is trivial; result should be trivial
  enum ops { int_to_v, v_to_int, uint_to_v, v_to_uint, neg };
  static ops string_to_op(std::string_view s) {
    if (s == "int_to_v")return int_to_v;
    if (s == "v_to_int")return v_to_int;
    if (s == "uint_to_v")return uint_to_v;
    if (s == "v_to_uint")return v_to_uint;
    if (s == "neg")return neg;
    THROW_UNIMPLEMENTED
  }

  uint64_t of_constant(uint64_t v) {
    switch (op) {
      case int_to_v:return uint64_t((int64_t(v) << 1) | 1);
      case v_to_int:return uint64_t(int64_t(v) >> 1);;
      case uint_to_v:return uint64_t((uint64_t(v) << 1) | 1);
      case v_to_uint:return uint64_t(uint64_t(v) >> 1);
      case neg:return uint64_t(-int64_t(v) + 2);
      default:THROW_UNIMPLEMENTED;
    }
  }
  static std::string_view ops_to_string(ops op) {
    switch (op) {
      case int_to_v:return "int_to_v";
      case v_to_int:return "v_to_int";
      case uint_to_v:return "uint_to_v";
      case v_to_uint:return "v_to_uint";
      case neg:return "neg";
      default:THROW_UNIMPLEMENTED;
    }
  }

  static void compile(std::ostream &os, ops op, std::string_view src, std::string_view dst) {
    switch (op) {

      case int_to_v: {
        os << "lea " << dst << ", [" << src << "+1+" << src << "]\n";
        return;
      };
      case v_to_int: {
        if (src != dst) {
          os << "mov " << dst << ", " << src << "\n";
        }
        os << "sar " << dst << ", 1 \n";
        return;
      };
      case neg: {
        if (src != dst) {
          os << "mov " << dst << ", " << src << "\n";
        }
        os << "neg " << dst << "\n";
        os << "add " << dst << ", 2 \n";
        return;
      }
      case uint_to_v: {
        THROW_UNIMPLEMENTED
        return;
      };
      case v_to_uint: {
        THROW_UNIMPLEMENTED
        return;
      };
      default: THROW_UNIMPLEMENTED
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
  static std::optional<ops> parse_op(std::string_view s) {
    if (s == "test")return test;
    if (s == "cmp")return cmp;
    return {};
  }
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
  std::vector<std::vector<var> > destroys; // destroys[i] must be destroyed before instruction i
  std::vector<std::pair<size_t, std::stringstream>> comments;
  void push_back(instruction::t &&i);
  scope &operator<<(instruction::t &&i);
  var ret;
  bool tight_inference();
  void print(std::ostream &os, size_t offset = 0) const;
  std::string to_string();
  void parse(parse::tokenizer &, std::unordered_map<std::string_view, var> &);
  ir::lang::var declare_constant(uint64_t);
  ir::lang::var declare_global(std::string_view);
  ir::lang::var declare_assign(rhs_expr::t &&);
  std::ostream &comment() { return comments.emplace_back(body.size(), std::stringstream{}).second; }
};

struct function : public scope {
  std::vector<var> args;
  std::string name;
  static function parse(std::string_view source);
  void setup_destruction();
  void parse(parse::tokenizer &);
  void print(std::ostream &os, size_t offset = 0) const;
  void compile(std::ostream &os);
  void pre_compile();
};

struct ternary {
  enum jmp_instr { jmp, jne, jle, jz, jnz }; //TODO: add others
  jmp_instr cond;
  scope nojmp_branch, jmp_branch;
  static jmp_instr parse_jinstr(std::string_view s) {
    if (s == "jmp")return jmp;
    if (s == "jne")return jne;
    if (s == "jle")return jle;
    if (s == "jz")return jz;
    if (s == "jnz")return jnz;
    THROW_INTERNAL_ERROR
  }
  std::string_view ops_to_string() {
    switch (cond) {

      case jmp:return "jmp";
      case jne:return "jne";

      case jle:return "jle";
      case jz:return "jz";
      case jnz:return "jnz";
      default:THROW_UNIMPLEMENTED
    }
  }
};

namespace parse {
enum token_type {
  IDENTIFIER,
  EQUAL,
  STAR,
  BRACKET_OPEN,
  BRACKET_CLOSE,
  CONSTANT,
  SEMICOLON,
  IF,
  THEN,
  ELSE,
  PARENS_OPEN,
  PARENS_CLOSE,
  CURLY_OPEN,
  CURLY_CLOSE,
  RETURN,
  ASSIGN,
  COMMA,
  COLON,
  END_OF_INPUT
};

namespace error {
class t : public std::runtime_error, public virtual util::message::base {
public:
  t() : std::runtime_error("ir::parse::error") {}
};


struct cannot_parse_anymore : public t, public util::message::error_token {
  cannot_parse_anymore(std::string_view filename,
                       std::string_view file) : util::message::error_token(std::string_view(file.end(), 0),
                                                                           file,
                                                                           filename) {}
  void describe(std::ostream &os) const {
    os << "cannot parse anymore";
  }
};

struct unexpected_token : public t, public util::message::error_token {
  unexpected_token(std::string_view filename,
                   std::string_view file,
                   std::string_view found) : util::message::error_token(found, file, filename) {}
  void describe(std::ostream &os) const {
    os << "token " << util::message::style::bold << token << util::message::style::clear << "was not expected here.";
  }
};

struct expected_token_found_another : public t, public util::message::error_token {
public:
  std::string_view expected;
  expected_token_found_another(std::string_view filename,
                               std::string_view file,
                               std::string_view expected,
                               std::string_view found)
      : util::message::error_token(found, file, filename), expected(expected) {}
  void describe(std::ostream &os) const {
    os << "token " << util::message::style::bold << expected << util::message::style::clear << ", but "
       << util::message::style::bold << token << util::message::style::clear << " was found";
  }
};

struct custom_msg : public t, public util::message::error_report_token_front_back {
  custom_msg(std::string_view front, std::string_view token, std::string_view back,
             std::string_view file, std::string_view filename) : util::message::error_report_token_front_back(front,
                                                                                                              token,
                                                                                                              back,
                                                                                                              file,filename) {}
};
}

namespace {
typedef std::pair<std::string_view, token_type> st;
}
constexpr auto tokens_map = util::make_array(
    st{"(", PARENS_OPEN},
    st{")", PARENS_CLOSE},
    st{"[", BRACKET_OPEN},
    st{"]", BRACKET_CLOSE},
    st{"{", CURLY_OPEN},
    st{"}", CURLY_CLOSE},
    st{"=", EQUAL}, st{":=", ASSIGN}, st{";", SEMICOLON},
    st{":", COLON},
    st{"if", IF}, st{"then", THEN},
    st{"else", ELSE}, st{"return", RETURN},
    st{"*", STAR}, st{",", COMMA});

struct token {

  std::string_view sv;
  token_type type;
};

class tokenizer {
public:
  tokenizer(const tokenizer &) = default;
  tokenizer(tokenizer &&) = default;
  tokenizer &operator=(const tokenizer &) = default;
  tokenizer &operator=(tokenizer &&) = default;
  explicit tokenizer(std::string_view source);
  token_type peek() const;
  token peek_full() const;
  std::string_view peek_sv() const;
  token pop();
  bool empty() const;
  void expect_pop(token_type);
  void expect_peek(token_type);
  void expect_peek_any_of(std::initializer_list<token_type>);
  void unexpected_token();
  void print_errors();
  std::string_view get_source() const;
private:
  void write_head();
  std::string_view to_parse, source;
  token head;
};

}

};

namespace std {
template<>
struct hash<ir::lang::var> {
  std::size_t operator()(const ir::lang::var &k) const {
    return k.id;
  }
};

}

#endif //COMPILERS_BML_LIB_IR_LANG_H_
