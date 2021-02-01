#include <iostream>
#include <cassert>
#include <variant>
#include <vector>
#include <util/util.h>
#include <memory>



/* TYPES */
/*
PE = possibly empty
// type_function (from monotypes^n -> monotypes)
      they have a unique name:string, and a n_args:int
      we can identify one by writing name/n_args

      - they can be primitives, like
        ~ int/0
        ~ bool/0
        ~ string/0
        ~ fun/2
        ~ unit/0
        ~ tuple<2>/2
        ~ tuple<n>/n
        (quite simple for now)
      - and they can be variants, like
        ~ list/1 = | Nil | Cons of 'a * 'a list
        ~ option/1 = | None | Some of 'a
        (they have a PE list of variants, each of which have
          - a tag_name
          - a PE list of typeexpr, using vars in [0,n_vars)
      - or they can be typedefs, which we ignore for now.

// a typeexpr (or mono-typeexpr) is either
  - a variable : int
  - a constructor : typeconstr with constructor.n_args typeexprs provided

// a poly-typeexpr is
 { - a set of universally quantified variables
   - a typeexpr using those variables
  }

  a poly-typeexpr can be put in normal form by
    1. not storing the variables - the set of variables is the set of variables occurring in the typeexpr
    2. relabel the empty variables by order of appearance

  We should store poly-typeexpr in normal form.

  A program that typechecks will assign poly-typeexpr to all the global universal_matchers
*/
namespace util {

}
//declaration of structs
namespace type_function {
struct t;
struct primitive;// : public t;
struct variant;// : public t;
}
namespace type_expr {
struct t;
struct variable;// : public t;
struct constr;// : public t;
}

//definition of structs, declaration of methods (and definition of some)
namespace type_function {

template<typename T>
struct c {
  const t *constr;
  std::vector<std::variant<c<T>, T>> args;
};
template<typename T>
using tree = std::variant<c<T>, T>;

struct t {
  const std::string_view name;
  const size_t n_args;
  constexpr t(std::string_view name, size_t n_args) : name(name), n_args(n_args) {}
  virtual void print_with_args(const std::vector<type_expr::ptr> &args,
                               std::ostream &os) const; //override for tuple, fun
  template<typename ...Types>
  type_expr::ptr operator()(Types &&... args) const;
  /*type_expr::ptr t::operator()(std::vector<type_expr::ptr> &&args) const {
  return std::make_unique<type_expr::constr>(( t*)this,std::move(args));
}*/

  template<typename T, typename ...Types>
  tree<T> of(Types &&... args) const {
    std::vector<tree<T>> args_;
    util::multi_emplace(args_, std::forward<Types &&>(args)...);
    assert(args_.size() == n_args);
    return c<T>{.constr = this, .args = std::move(args_)};
  }

};

struct primitive : public t {
  using t::t;
};

static constexpr primitive tc_int("int", 0);
static constexpr primitive tc_string("string", 0);
static constexpr primitive tc_bool("bool", 0);
static constexpr primitive tc_unit("unit", 0);

struct fun : public primitive {
  constexpr fun() : primitive("fun", 2) {}
  void print_with_args(const std::vector<type_expr::ptr> &args, std::ostream &os) const final;
};

struct tc_tuple_t : public primitive {
  constexpr tc_tuple_t(size_t n_args) : primitive("tuple", n_args) {}
  void print_with_args(const std::vector<type_expr::ptr> &args, std::ostream &os) const final;
};

static constexpr fun tc_fun;
/*
namespace detail{
template<size_t N,size_t At_least>
struct check_at_least{
  static_assert(N>=At_least,"N should be at least At_least");
  static constexpr size_t n = N;
};
}
template<size_t N>
static constexpr primitive tc_tuple("tuple",detail::check_at_least<N,2>::n);
 */
tc_tuple_t &tc_tuple(size_t n) {
  static std::unordered_map<size_t, tc_tuple_t> tuples;
  return tuples.try_emplace(n, tc_tuple_t(n)).first->second;
}
struct variant : public t {
  struct constr {
    static size_t fresh_id() {
      static size_t next_id = 101;
      next_id+=2;
      return next_id;
    }
    const size_t tag_id;
    const std::string_view name;
    std::vector<type_expr::t> args;//PE
    template<typename... TExprs>
    /*TODO-C++20: constexpr*/ constr(std::string_view name,TExprs&& ... texprs) : tag_id(fresh_id()),name(name){
      util::multi_emplace(args,std::forward<TExprs &&>(texprs)...);
    }
    template<typename T>
    std::vector<tree<T>> of(const std::vector<tree<T>>& subs)const;
  };
  std::vector<constr> constructors; //PE
template<typename... Constructors>
  /*TODO-C++20: constexpr*/ variant(std::string_view name, size_t n_args, Constructors&& ... cs) : t(name,n_args) {
    util::multi_emplace(constructors,std::forward<Constructors &&>(cs)...);
  }
  //TODO: giving tree for each arg, and id of appropriate constructor, return a vector of trees describing the associate types
  template<typename T, typename ...Types>
  std::vector<tree<T>> constructor_of(const constr& c,Types &&... args) const {
    std::vector<tree<T>> args_;
    util::multi_emplace(args_, std::forward<Types &&>(args)...);
    assert(args_.size() == n_args);
    return c.of<T>(args_);
  }
};


}
namespace type_expr {
typedef type_function::tree<size_t> tree;

struct variable {
  size_t id;
  variable(size_t id) : id(id) {}
  void print(std::ostream &os) const  {
    if (id < 26)os << '\'' << char('a' + id);
    else os << "'_" << id;
  }
};
struct constr {
  const type_function::t *constructor;
  std::vector<t> args; //args.size()==constructor->n_args
  constr(const type_function::t *constructor, std::vector<t> &&args) : constructor(constructor), args(std::move(args)) {
    assert(this->args.size() == constructor->n_args);
  }

  void print(std::ostream &os) const {
    return constructor->print_with_args(args, os);
  }
};
struct t : public std::variant<variable,constr> {
  typedef std::variant<variable,constr> base;
  using base::base;
  void print(std::ostream &os) const {
    std::visit(util::overloaded{
      [&os](variable v){v.print(os);},
      [&os](const constr& c){c.print(os);}
      },*static_cast<const base *>(this));
  };
};
}
//definition of remaining methods
namespace type_function {

static /*TODO-C++20: constexpr*/ variant tc_option("option",1,variant::constr("None"),variant::constr("Some",type_expr::variable::make(0)));
const variant& __make_tc_list(){
  static bool not_called_yet = true;
  assert(not_called_yet);
  not_called_yet= false;
  static /*TODO-C++20: constexpr*/ variant tc_list("list",1,variant::constr("Nil"),variant::constr("Cons",   type_expr::variable::make(0) ));
  tc_list.constructors.back().args.push_back(tc_list(type_expr::variable::make(0)));
  return tc_list;

}
static /*TODO-C++20: constexpr*/const variant& tc_list = __make_tc_list();

template<typename... Types>
type_expr::ptr t::operator()(Types &&... args) const {
  std::vector<type_expr::ptr> args_;
  util::multi_emplace(args_, std::forward<Types &&>(args)...);
  assert(args_.size() == n_args);
  return std::make_unique<type_expr::constr>((t *) this, std::move(args_));
}
void t::print_with_args(const std::vector<type_expr::ptr> &args, std::ostream &os) const {
  assert(args.size() == n_args);
  if (args.empty()) {
    os << name;
  } else if (args.size() == 1) {
    args.front()->print(os);
    os << " " << name;
  } else {
    os << "(";
    bool comma = false;
    for (const auto &p : args) {
      if (comma)os << ", ";
      comma = true;
      p->print(os);
    }
    os << ") " << name;
  }
}

void fun::print_with_args(const std::vector<type_expr::ptr> &args, std::ostream &os) const {
  assert(args.size() == 2);
  os << "(";
  args[0]->print(os);
  os << " -> ";
  args[1]->print(os);
  os << ")";
}

void tc_tuple_t::print_with_args(const std::vector<type_expr::ptr> &args, std::ostream &os) const {
  assert(args.size() >= 2);
  os << "(";
  bool comma = false;
  for (const auto &p : args) {
    if (comma)os << " * ";
    comma = true;
    p->print(os);
  }
  os << ")";
}
template<typename T>
std::vector<tree<T>> variant::constr::of(const std::vector<tree<T>> &subs) const {
  std::vector<tree<T>> rets;
  for(const auto &p : args)rets.emplace_back(p->to_tree_subs(subs));
  return rets;
}
}
std::ostream& operator<<(std::ostream& os, const type_expr::ptr& p){
  p->print(os);
  return os;
}

//TODO:
// 1. understand better parse of Tag / Constructors
// 2. then re-implement compiler so that the tag actually contains all arguments together
// 3. reimplement type parsing/constructor parsing
// 4. add typecheck to build pass

void test_types() {
  const type_function::t &c = type_function::tc_tuple(3);
  std::cout << c.name << "/" << c.n_args << std::endl;
  static constexpr auto &make = type_expr::variable::make;
  type_expr::ptr f = make(3);
  f->print(std::cout);
  std::cout << std::endl;
  f->poly_normalize();
  f->print(std::cout);
  std::cout << std::endl;
  type_expr::ptr t = c(make(7), make(2), c(type_function::tc_fun(make(2), make(7)), make(7), type_function::tc_string()));
  t->print(std::cout);
  std::cout << std::endl;
  t->poly_normalize();
  t->print(std::cout);
  std::cout << std::endl;

}

/* INFERENCE */

typedef type_function::t constr;
struct arena {
  typedef size_t id_t;
  struct boss_node {
    size_t id;
    size_t rank;
    const constr *def; //NULL if pure
    std::vector<id_t> args; //n_args.size()==def->n_args
  };
  struct child_node {
    mutable id_t father_id;
  };
  typedef std::variant<boss_node, child_node> node;
  //static_assert(sizeof(node)==48);
  std::vector<node> nodes;
  boss_node &boss(id_t x) {
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
  const boss_node &boss(id_t x) const {
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

  id_t fresh() {
    nodes.emplace_back(boss_node{.id = nodes.size(), .rank=nodes.size(), .def=nullptr, .args={}});
    return nodes.size() - 1;
  }

  id_t construct(const type_function::tree<id_t> &i) {

    return std::visit(util::overloaded{
        [](id_t id) -> id_t { return id; },
        [&](const type_function::c<id_t> &c) -> id_t {
          std::vector<id_t> args;
          for (const auto &x : c.args)args.push_back(construct(x));
          nodes.emplace_back(boss_node{.id = nodes.size(), .rank=0, .def=c.constr, .args=std::move(args)});
          return nodes.size() - 1;
        }
    }, i);
  }

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
  type_expr::ptr to_typeexpr_(const boss_node &b) const {
    if (b.def == nullptr)return std::make_unique<type_expr::variable>(b.id);
    assert(b.args.size() == b.def->n_args);
    std::vector<type_expr::ptr> args;
    for (id_t x : b.args)args.emplace_back(to_typeexpr(x));
    return std::make_unique<type_expr::constr>(b.def, std::move(args));
  }

  bool occurs_in(const boss_node &small, const boss_node &big) const {
    if (&small == &big)return true;
    for (id_t i : big.args)if (occurs_in(small, boss(i)))return true;
    return false;
  }

  void print(id_t i) const {
    std::unordered_map<size_t, size_t> names;
    print_(boss(i), names);
    std::cout << std::endl;
  }
  type_expr::ptr to_typeexpr(id_t i) const {
    return to_typeexpr_(boss(i));
  }
  bool unify(id_t x, id_t y) {
    boss_node *bx = &boss(x), *by = &boss(y);
    if (bx == by)return true; //same group
    if (bx->def == nullptr)std::swap(bx, by);
    //Now we have 3 cases
    //1. both pure -> always ok
    if (bx->def == nullptr && by->def == nullptr) {
      if (bx->rank < by->rank)std::swap(bx, by);
      if (bx->rank == by->rank)++bx->rank;
      nodes[by->id] = child_node{.father_id = bx->id};
      return true;
    }
    //2. bx is not pure, by is pure -> always ok (UNLESS occurs check)
    if (by->def == nullptr) {
      assert(bx->def);
      if (occurs_in(*by, *bx)) {
        std::cerr << "occurs chek failed" << std::endl;
        return false;
      }
      if (bx->rank < by->rank) {
        std::swap(*bx, *by);
        std::swap(bx->id, by->id);
        std::swap(bx->rank, by->rank);
        std::swap(bx,by);
      }
      if (bx->rank == by->rank)++bx->rank;
      nodes[by->id] = child_node{.father_id = bx->id};
      return true;
    }
    //3. both not pure
    {
      assert(bx->def);
      assert(by->def);
      if (bx->def != by->def) {
        std::cerr << "cannot unify " << bx->def->name << " with " << by->def->name << std::endl;
        return false;
      }
      for (size_t i = 0; i < bx->def->n_args; ++i)if (!unify(bx->args.at(i), by->args.at(i)))return false;
      if (bx->rank < by->rank)std::swap(bx, by);
      if (bx->rank == by->rank)++bx->rank;
      nodes[by->id] = child_node{.father_id = bx->id};
      return true;
    }
    //TODO: have you performed the occurs check?
    THROW_INTERNAL_ERROR
  }

};
using namespace type_function;

void test1() {
  arena a;
  a.fresh();
  id_t t1 = a.fresh(), t2 = a.fresh();
  auto t3 = a.construct(tc_tuple(2).of<arena::id_t>(t1, t2));
  a.print(t1);
  a.print(t2);
  a.print(t3);
  type_expr::ptr te = a.to_typeexpr(t3);
  std::cout << "unnormalized te: " << te << std::endl;
  te->poly_normalize();
  std::cout << "  normalized te: " << te << std::endl;
  a.unify(t1, t2);
  a.print(t3);
  std::cout << "unnormalized te: " << a.to_typeexpr(t3) << std::endl;
}

void test2() {
  // consider the following code
  // let twice f x = f (f x) ;;
  // f : t_f
  // x : t_x
  // f x : t_f_x
  // f (f x) : t_body
  // twice : t_twice
  arena a;
  auto t_f = a.fresh(), t_x = a.fresh(), t_body = a.fresh(), t_twice = a.fresh(), t_f_x = a.fresh();
  assert(a.unify(t_f, a.construct(tc_fun.of<arena::id_t>(t_x,t_f_x))));
  assert(a.unify(t_f, a.construct(tc_fun.of<arena::id_t>(t_f_x,t_body))));
  assert(a.unify(t_twice, a.construct(tc_fun.of<arena::id_t>(t_f_x, tc_fun.of<arena::id_t>(t_x,t_body) ))));

  std::cout << "f : ";
  a.print(t_f);
  std::cout << "x : ";
  a.print(t_x);
  std::cout << "twice : ";
  a.print(t_twice);

}
/*
void test3() {
  // let f x = x
  arena a;
  auto t_f = a.fresh(), t_x = a.fresh();
  assert(a.unify(t_f, a.construct({.c= function_c, .args={t_x, t_x}})));
  std::cout << "f : ";
  a.print(t_f);

}

void test4() {
  // let apply f x = f x
  arena a;
  auto t_f = a.fresh(), t_x = a.fresh(), t_body = a.fresh(), t_apply = a.fresh();
  assert(a.unify(t_apply, a.construct({.c= function_c, .args={t_f, inp{.c=function_c, .args={t_x, t_body}}}})));
  assert(a.unify(t_f, a.construct({.c = function_c, .args={t_x, t_body}})));
  std::cout << "apply : ";
  a.print(t_apply);
}

void test5() {
  //let rec g f x = f x;g  f ;;
  arena a;
  auto t_f = a.fresh(), t_x = a.fresh(), t_body = a.fresh(), t_g = a.fresh(), t_f_x = a.fresh(), t_g_f = a.fresh();
  assert(a.unify(t_g, a.construct({.c= function_c, .args={t_f, inp{.c=function_c, .args={t_x, t_body}}}})));
  assert(a.unify(t_f, a.construct({.c = function_c, .args={t_x, t_f_x}})));
  assert(a.unify(t_g, a.construct({.c = function_c, .args={t_f, t_g_f}})));
  assert(a.unify(t_g_f, t_body));
  //TODO: occurs check
  //std::cout << "g : ";a.print(t_g);

}

void test5_err() {
  //let rec g f x = f x;g  f ;;
  arena a;
  auto t_f = a.fresh(), t_x = a.fresh(), t_body = a.fresh(), t_g = a.fresh(), t_f_x = a.fresh(), t_g_f = a.fresh();
  assert(a.unify(t_g, a.construct({.c= function_c, .args={t_f, inp{.c=function_c, .args={t_x, t_body}}}})));
  assert(a.unify(t_f, a.construct({.c = function_c, .args={t_x, t_f_x}})));
  assert(a.unify(t_g, a.construct({.c = function_c, .args={t_g, t_g_f}}))); //HERE is different and fails
  //assert(a.unify(t_g_f,t_body));

  std::cout << "g : ";
  a.print(t_g);

}

void test6() {
  //let f x = f x
  arena a;
  auto t_f = a.fresh(), t_x = a.fresh(), t_f_x = a.fresh();
  assert(a.unify(t_f, a.construct({.c = function_c, .args={t_x, t_f_x}})));
  std::cout << "f : ";
  a.print(t_f);
}

void test7() {
  // (x + y) +. z
  arena a;
  auto t_int = a.construct({.c = int_c});
  auto t_float = a.construct({.c = float_c});
  auto t_int_plus = a.construct_fun(t_int, a.construct_fun(t_int, t_int));
  auto t_float_plus = a.construct_fun(t_float, a.construct_fun(t_float, t_float));
  auto t_x = a.fresh(), t_y = a.fresh(), t_xy = a.fresh();
  std::cout << "(+) : ";
  a.print(t_int_plus);
  auto t_px = a.fresh();
  a.unify(t_int_plus, a.construct_fun(t_x, t_px));
  std::cout << "(+) x : ";
  a.print(t_px);
  std::cout << "x : ";
  a.print(t_x);
  std::cout << "y : ";
  a.print(t_y);
  a.unify(t_px, a.construct_fun(t_y, t_xy));
  std::cout << "x : ";
  a.print(t_x);
  std::cout << "y : ";
  a.print(t_y);
  auto t_pxy = a.fresh();
  a.unify(t_float_plus, a.construct_fun(t_xy, t_pxy));
}
*/



void test8(){
  // let sum l = match l with
  //    | Nil -> 0
  //    | Cons x,xs -> x + (sum xs) (* If this
  arena a;
  auto t_Nil = a.fresh(), t_l = a.fresh();
  a.unify(t_Nil,t_l);
  a.unify(t_Nil,a.construct(tc_list.of<arena::id_t>(a.fresh())));
  auto t_sum = a.fresh(), t_body = a.fresh();
  a.unify(t_sum,a.construct(tc_fun.of<arena::id_t>(t_l,t_body)));
  a.unify(t_body,a.construct(tc_int.of<arena::id_t>()));
  auto t_Cons_x_xs = a.fresh(),t_x = a.fresh(),t_xs = a.fresh();
  a.unify(t_Cons_x_xs,t_l);
  //tc_list.constructors.at(1).args.at(0)->
  //a.unify(t_x,);
  //TODO: terminate
  std::cout <<"xs : ";a.print(t_xs);
  std::cout <<"sum : ";a.print(t_sum);
}

void test9(){
  static variant tc_strange("strange",1,variant::constr("A",type_expr::variable::make(0)),variant::constr("B",type_expr::variable::make(0)));
  /*
 * TEST
 * type 'a strange = | A of 'a | B of 'a ;;
 * let f x = match x with
 *      | A y -> Some (y+1)
 *      | B z -> None ;; TODO: algo should be able to tell z==int
 */
  arena a;
  auto t_x = a.fresh();
  auto t_z = a.fresh(), t_B_z = a.fresh();
  a.unify(t_B_z,t_x);
  {
    //do B z
    //1. gen fresh args
    auto _a = a.fresh();
    //2. instantiate that variant with those fresh args
    a.unify(t_B_z,a.construct(tc_strange.of<arena::id_t>(_a)));
    //3. get the dependent type for all arguments of that constructors
    auto Bvarg =  tc_strange.constructor_of<arena::id_t>(tc_strange.constructors[1],_a);
    assert(Bvarg.size()==tc_strange.constructors[1].args.size());
    //4. unify each one of them with the matchers
    a.unify(t_z,a.construct(Bvarg.at(0)));
  }
  auto t_y = a.fresh(), t_A_y = a.fresh();
  a.unify(t_A_y,t_x);
  {
    //do A y
    //similar
    auto _a = a.fresh();
    a.unify(t_A_y,a.construct(tc_strange.of<arena::id_t>(_a)));
    auto Avarg =  tc_strange.constructor_of<arena::id_t>(tc_strange.constructors[0],_a);
    assert(Avarg.size()==1);
    a.unify(t_y,a.construct(Avarg.at(0)));
  }
  a.unify(t_y,a.construct(tc_int.of<size_t>()));
  auto t_body = a.fresh();


  std::cout <<"z : ";a.print(t_z);
}
int main() {
//  test_types();
//
//  test1();
//  test2();
  test9();
}
