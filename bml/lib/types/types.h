#ifndef COMPILERS_BML_LIB_AST_TYPES_H_
#define COMPILERS_BML_LIB_AST_TYPES_H_
#include <string_view>
#include <vector>
#include <unordered_map>
#include <cassert>
#include <variant>
#include <iostream>
#include <memory>
#include <util/util.h>

namespace type {

namespace expression {
struct t;
}

namespace function {
struct t;
typedef std::unique_ptr<t> ptr;
}

namespace function {
struct t { //type function, that takes monotypes and produce a monotype
  const std::string_view name;
  const size_t n_args;
  t() = delete;
  explicit constexpr t(std::string_view name, size_t n_args) : name(name), n_args(n_args) {}
  virtual void print_with_args(const std::vector<expression::t> &args,
                               std::ostream &os) const = 0; //overridden for tuple, fun
  template<typename ...Types>
  expression::t operator()(Types &&... args) const;

};

struct primitive : public t { //primitives are the type-functions taking no arg and non defined to be a variant
  primitive() = delete;
  primitive(const primitive &) = delete;
  primitive(primitive &&) = delete;
  explicit constexpr primitive(std::string_view name) : t(name, 0) {}
  void print_with_args(const std::vector<expression::t> &args,
                       std::ostream &os) const final;
};

extern primitive tf_char, tf_int, tf_bool, tf_string, tf_unit, tf_time, tf_file;
struct tf_fun_t : public t {
  tf_fun_t(const tf_fun_t &) = delete;
  tf_fun_t(tf_fun_t &&) = delete;
  explicit constexpr tf_fun_t() : t("fun", 2) {}
  void print_with_args(const std::vector<expression::t> &args, std::ostream &os) const final;
};
extern tf_fun_t tf_fun;

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
    constr(std::string_view name, const variant *parent_tf) : tag_id(fresh_id()), name(name), parent_tf(parent_tf) {}
  };
  std::vector<constr> constructors; //PE
  using t::t;
  variant(std::string_view name, size_t n, std::vector<constr> &&cs) : t(name, n),
                                                                       constructors(std::move(cs)) { __own_constructors(); }
private:
  void __own_constructors() { for (constr &c : constructors)c.parent_tf = this; }
public:
  variant(const variant &v) : t(v), constructors(v.constructors) { __own_constructors(); }
  variant(variant &&v) : t(std::move(v)), constructors(std::move(v.constructors)) { __own_constructors(); }
  void print_with_args(const std::vector<expression::t> &args,
                       std::ostream &os) const final;
  typedef std::unique_ptr<variant> ptr;
};

}

namespace expression {
struct variable {
  size_t id;
  constexpr variable(size_t id) : id(id) {}
  void print(std::ostream &os) const;
};

namespace placeholder {
static constexpr variable _a(0), _b(1), _c(2), _d(3);
}

struct application {
  const function::t *f;
  std::vector<t> args; //args.size()==f->n_args
  application(const function::t *f, std::vector<t> &&args = {}) : f(f), args(std::move(args)) {
    assert(this->args.size() == f->n_args);
  }
  application(const application&) = default;
  application(application&& o) : f(o.f), args(std::move(o.args)) {
    o.args.clear();
    o.f = nullptr;
  }

  void print(std::ostream &os) const;

};
struct t : public std::variant<variable, application> {
  typedef std::variant<variable, application> base;
  using base::base;
  explicit constexpr t(const function::t &f) : base(std::in_place_type_t<application>(), &f) {
    assert(f.n_args == 0);
  }
  explicit t(const function::t *f, std::vector<t> &&args = {});
  void print(std::ostream &os) const;
  bool is_poly_normalized() const;
  bool is_valid() const;
  size_t poly_n_args() const;
  void poly_normalize();
private:
  bool check_poly_normalized(size_t &next_fresh) const;
  void execute_poly_normalize(std::unordered_map<size_t, size_t> &relabel);
};
}

typedef std::unordered_map<std::string_view, const function::variant::constr *> constr_map;
typedef std::unordered_map<std::string_view, const function::t *> type_map;
type_map make_default_type_map();
template<typename... Types>
expression::t function::t::operator()(Types &&... args) const {
  std::vector<expression::t> args_;
  util::multi_emplace(args_, std::forward<Types &&>(args)...);
  assert(args_.size() == n_args);
  return expression::application(this, std::move(args_));
}

struct arena {
  typedef size_t idx_t;
  struct boss_node {
    size_t id;
    size_t rank;
    const type::function::t *def; //NULL if pure
    std::vector<idx_t> args; //n_args.size()==def->n_args
  };
  struct child_node {
    mutable idx_t father_id;
  };
  typedef std::variant<boss_node, child_node> node;
  //static_assert(sizeof(node)==48);
  std::vector<node> nodes;
  boss_node &boss(idx_t x) {
    assert(x < nodes.size());
    return std::visit(util::overloaded{
        [&](boss_node &n) -> boss_node & { return n; },
        [&](child_node &c) -> boss_node & {
          boss_node &n = boss(c.father_id);
          c.father_id = n.id;
          return n;
        },
    }, nodes[x]);
  }
  const boss_node &boss(idx_t x) const {
    assert(x < nodes.size());
    return std::visit(util::overloaded{
        [&](const boss_node &n) -> const boss_node & { return n; },
        [&](const child_node &c) -> const boss_node & {
          const boss_node &n = boss(c.father_id);
          c.father_id = n.id;
          return n;
        },
    }, nodes[x]);
  }

  idx_t fresh() {
    assert_consistency();
    nodes.emplace_back(boss_node{.id = nodes.size(), .rank=nodes.size(), .def=nullptr, .args={}});
    return nodes.size() - 1;
    assert_consistency();
  }

  std::vector<idx_t> fresh_n(size_t n) {
    assert_consistency();
    std::vector<idx_t> v(n);
    for (size_t i = 0; i < n; ++i)v.at(i) = fresh();
    return v;
    assert_consistency();
  }

  idx_t apply(const function::t *f, std::vector<idx_t> &&args) {
    assert(f!= nullptr);
    assert(f->n_args == args.size());
    assert_consistency();
    nodes.emplace_back(boss_node{.id = nodes.size(), .rank=0, .def=f, .args=std::move(args)});
    return nodes.size() - 1;
    assert_consistency();
  }

  idx_t type_with_args(const expression::t &t, const std::vector<idx_t> &args) {
    assert_consistency();
    return std::visit(util::overloaded{
        [&](const expression::variable &v) -> idx_t {
          assert(v.id < args.size());
          assert(args.at(v.id) < nodes.size());
          assert_consistency();
          return args.at(v.id);
        },
        [&](const expression::application &app) -> idx_t {
          std::vector<idx_t> app_args;
          for (const auto &t : app.args)app_args.push_back(type_with_args(t, args));
          assert_consistency();
          return apply(app.f, std::move(app_args));
        }
    }, (const expression::t::base &) t);
  }

  /* idx_t construct(const type_function::tree<idx_t> &i) {

     return std::visit(util::overloaded{
         [](idx_t id) -> idx_t { return id; },
         [&](const type_function::c<idx_t> &c) -> idx_t {
           std::vector<idx_t> args;
           for (const auto &x : c.args)args.push_back(construct(x));
           nodes.emplace_back(boss_node{.id = nodes.size(), .rank=0, .def=c.constr, .args=std::move(args)});
           return nodes.size() - 1;
         }
     }, i);
   }*/
/*
  static void print_pure(size_t n) {
    std::cout << '\'';
    if (n < 26) {
      std::cout << char('a' + n);
    } else {
      std::cout << '_' << n;
    }
  }
  void print_(const boss_node &b, std::unordered_map<size_t, size_t> &names) const {
    if (b.def == nullptr) {
      return print_pure(names.try_emplace(b.id, names.size()).first->second);
    }

    assert(b.args.size() == b.def->n_args);

    if (b.def->n_args == 0) {
      std::cout << b.def->name;
    } else if (b.def == &type_function::tc_fun) {
      // special case for c_function
      std::cout << "(";
      print_(boss(b.args[0]), names);
      std::cout << " -> ";
      print_(boss(b.args[1]), names);
      std::cout << ") ";
    } else if (b.def->name == "tuple") {
      //special case for tuple
      bool comma = false;
      std::cout << "(";
      for (size_t i : b.args) {
        if (comma)std::cout << " * ";
        comma = true;
        print_(boss(i), names);
      }
      std::cout << ")";
    } else {
      bool comma = false;
      std::cout << "(";
      for (size_t i : b.args) {
        if (comma)std::cout << ", ";
        comma = true;
        print_(boss(i), names);
      }
      std::cout << ") " << b.def->name;
    }

  }
  */
  expression::t to_typeexpr_(const boss_node &b) const {
    if (b.def == nullptr)return expression::variable(b.id);
    assert(b.args.size() == b.def->n_args);
    std::vector<expression::t> args;
    for (idx_t x : b.args)args.emplace_back(to_typeexpr(x));
    return expression::application(b.def, std::move(args));
  }

  bool occurs_in(const boss_node &small, const boss_node &big) const {
    if (&small == &big)return true;
    for (idx_t i : big.args)if (occurs_in(small, boss(i)))return true;
    return false;
  }

  void assert_consistency() const {
    for (size_t i = 0; i < nodes.size(); ++i)
      std::visit(
          util::overloaded{
              [&](const boss_node &bn) {
                assert(bn.id == i);
                if (bn.def == nullptr) {
                  assert(bn.args.empty());
                } else {
                  assert(bn.args.size() == bn.def->n_args);
                }
              },
              [&](const child_node &cn) {
                assert(cn.father_id != i);
                assert(cn.father_id < nodes.size());
              }
          }, nodes.at(i));
  }

  /*void print(idx_t i) const {
    std::unordered_map<size_t, size_t> names;
    print_(boss(i), names);
    std::cout << std::endl;
  }*/
  expression::t to_typeexpr(idx_t i) const {
    return to_typeexpr_(boss(i));
  }
  bool unify(idx_t x, idx_t y) {
    assert_consistency();
    boss_node *bx = &boss(x), *by = &boss(y);
    if (bx == by)return true; //same group
    if (bx->def == nullptr)std::swap(bx, by);
    //Now we have 3 cases
    //1. both pure -> always ok
    if (bx->def == nullptr && by->def == nullptr) {
      if (bx->rank < by->rank)std::swap(bx, by);
      if (bx->rank == by->rank)++bx->rank;
      nodes[by->id] = child_node{.father_id = bx->id};
      assert_consistency();
      return true;
    }
    //2. bx is not pure, by is pure -> always ok (UNLESS occurs check)
    if (by->def == nullptr) {
      assert(bx->def);
      if (occurs_in(*by, *bx)) {
        std::cerr << "occurs chek failed" << std::endl;
        throw std::runtime_error("occurs check failed");
        return false;
      }
      if (bx->rank < by->rank) {
        std::swap(*bx, *by);
        std::swap(bx->id, by->id);
        std::swap(bx->rank, by->rank);
        std::swap(bx, by);
      }
      if (bx->rank == by->rank)++bx->rank;
      nodes[by->id] = child_node{.father_id = bx->id};
      assert_consistency();
      return true;
    }
    //3. both not pure
    {
      assert(bx->def);
      assert(by->def);
      if (bx->def != by->def) {
        std::cerr << "cannot unify " << bx->def->name << " with " << by->def->name << std::endl;
        throw std::runtime_error("could not unify");
        return false;
      }
      for (size_t i = 0; i < bx->def->n_args; ++i)if (!unify(bx->args.at(i), by->args.at(i)))return false;
      if (bx->rank < by->rank)std::swap(bx, by);
      if (bx->rank == by->rank)++bx->rank;
      nodes[by->id] = child_node{.father_id = bx->id};
      assert_consistency();
      return true;
    }
    //TODO: have you performed the occurs check?
    THROW_INTERNAL_ERROR
  }

};

}

std::ostream &operator<<(std::ostream &os, const type::expression::t &t);

#endif //COMPILERS_BML_LIB_AST_TYPES_H_
