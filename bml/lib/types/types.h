#ifndef COMPILERS_BML_LIB_AST_TYPES_H_
#define COMPILERS_BML_LIB_AST_TYPES_H_
#include <string_view>
#include <vector>
#include <unordered_map>
#include <cassert>
#include <variant>
#include <iostream>

namespace type {

namespace expression {
struct t;
}

namespace function {
struct t;
}

namespace function {
struct t { //type function, that takes monotypes and produce a monotype
  const std::string_view name;
  const size_t n_args;
  explicit constexpr t(std::string_view name, size_t n_args) : name(name), n_args(n_args) {}
  virtual void print_with_args(const std::vector<expression::t> &args,
                               std::ostream &os) const = 0; //overridden for tuple, fun
};

struct primitive : public t { //primitives are the type-functions taking no arg and non defined to be a variant
  explicit constexpr primitive(std::string_view name) : t(name, 0) {}
  void print_with_args(const std::vector<expression::t> &args,
                       std::ostream &os) const final;
};

static constexpr primitive tf_int("int"), tf_bool("bool"), tf_string("string"), tf_unit("unit"), tf_time("time"),
    tf_file("file");
struct tf_fun_t : public t {
  explicit constexpr tf_fun_t() : t("fun", 2) {}
  void print_with_args(const std::vector<expression::t> &args, std::ostream &os) const final;
};
static constexpr tf_fun_t tf_fun;

struct tf_tuple_t : public t {
  explicit constexpr tf_tuple_t(size_t n_args) : t("tuple", n_args) {}
  void print_with_args(const std::vector<expression::t> &args, std::ostream &os) const final;
};
tf_tuple_t &tf_tuple(size_t n);

struct variant : public t {
  struct constr {
    static size_t fresh_id() {
      static size_t next_id = 101;
      next_id += 2;
      return next_id;
    }
    const size_t tag_id;
    const std::string_view name;
    std::vector<expression::t> args;//PE
    const variant *parent_tf = nullptr;
  };
  std::vector<constr> constructors; //PE
  variant(std::string_view name, size_t n, std::vector<constr> &&cs) : t(name, n),
                                                                       constructors(std::move(cs)) { __own_constructors(); }
private:
  void __own_constructors() { for (constr &c : constructors)c.parent_tf = this; }
public:
  variant(const variant& v) : t(v), constructors(v.constructors) {__own_constructors();}
  variant(variant&& v) : t(std::move(v)), constructors(std::move(v.constructors)) {__own_constructors();}
  void print_with_args(const std::vector<expression::t> &args,
                       std::ostream &os) const final;
};

}

namespace expression {
struct variable {
  size_t id;
  variable(size_t id) : id(id) {}
  void print(std::ostream &os) const;
};
struct application {
  const function::t *f;
  std::vector<t> args; //args.size()==f->n_args
  application(const function::t *f, std::vector<t> &&args = {}) : f(f), args(std::move(args)) {
    assert(this->args.size() == f->n_args);
  }

  void print(std::ostream &os) const;

};
struct t : public std::variant<variable, application> {
  typedef std::variant<variable, application> base;
  using base::base;
  explicit t(const function::t *f, std::vector<t> &&args = {});
  void print(std::ostream &os) const;
  bool is_poly_normalized() const;
  void poly_normalize();
private:
  bool check_poly_normalized(size_t &next_fresh) const;
  void execute_poly_normalize(std::unordered_map<size_t, size_t> &relabel) const;
};
}

}

std::ostream &operator<<(std::ostream &os, const type::expression::t &t);

#endif //COMPILERS_BML_LIB_AST_TYPES_H_
