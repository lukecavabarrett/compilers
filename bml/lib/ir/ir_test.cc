#include <ir/lang.h>
#include <gtest/gtest.h>

namespace ir::lang {
TEST(Syntax, Assignment) {
  scope s;
  var x;
  s << (x.assign(argv_var[2]));
}

TEST(Build, PassingConstant) {

  scope s;
  var x;
  s << (x.assign(34));
  for (int i = 0; i < 10; ++i) {
    var y;
    s << (y.assign(x));
    x = y;
  }
  s.ret = x;
  std::cout << "-----------" << std::endl;

  s.print(std::cout);
  std::cout << "-----------" << std::endl;

  s.compile_as_function(std::cout);
}

TEST(Build, VarPass) {
  scope s;
  var x;
  s << (x.assign(argv_var[3]));
  for (int i = 0; i < 10; ++i) {
    var y;
    s << (y.assign(x));
    x = y;
  }
  s.ret = x;
  std::cout << "-----------" << std::endl;

  s.print(std::cout);
  std::cout << "-----------" << std::endl;

  s.compile_as_function(std::cout);
}

TEST(Build, IntMin) {
  scope s;
  var x_sh("x_sh"), x("x"), next_arg("next_arg"), y_sh("y_sh"), y("y"), z("z");
  std::cout << "x:" << x.id << " y:"<<y.id << std::endl;
  s << (x_sh.assign(argv_var[2]));
  s << (x.assign(x_sh)); //TODO: sal
  s << (next_arg.assign(argv_var[3]));
  s << (y_sh.assign(next_arg[2]));
  s << (y.assign(y_sh)); // TODO: sal
  s << (instruction::cmp_vars{.v1 = y, .v2 = x, .op = instruction::cmp_vars::cmp});
  auto t = std::make_unique<ternary>();
  t->cond = ternary::jle;
  t->nojmp_branch.ret = y;
  t->jmp_branch.ret = x;
  s << (z.assign(std::move(t)));
  s.ret = z;
  std::cout << "-----------" << std::endl;

  s.print(std::cout);
  std::cout << "-----------" << std::endl;

  s.compile_as_function(std::cout);
}

TEST(Build, IntMinPlus1) {
  scope s;
  var x_sh("x_sh"), x("x"), next_arg("next_arg"), y_sh("y_sh"), y("y"), z("z");
  std::cout << "x:" << x.id << " y:"<<y.id << std::endl;
  s << (x_sh.assign(argv_var[2]));
  s << (x.assign(x_sh)); //TODO: sal
  s << (next_arg.assign(argv_var[3]));
  s << (y_sh.assign(next_arg[2]));
  s << (y.assign(y_sh)); // TODO: sal
  s << (instruction::cmp_vars{.v1 = x, .v2 = y, .op = instruction::cmp_vars::cmp});
  auto t = std::make_unique<ternary>();
  t->cond = ternary::jle;
  t->nojmp_branch.ret = x;
  t->jmp_branch.ret = y;
  s << (z.assign(std::move(t)));
  var p1("plus") ;
  var one("one");
  s << (one.assign(1));
  s << (p1.assign(rhs_expr::binary_op{.op = rhs_expr::binary_op::add,.x1 = z,.x2 = one}));
  s.ret = p1;
  std::cout << "-----------" << std::endl;

  s.print(std::cout);
  std::cout << "-----------" << std::endl;

  s.compile_as_function(std::cout);
}

}
