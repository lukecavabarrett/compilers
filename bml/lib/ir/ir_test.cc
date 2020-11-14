#include <ir/lang.h>
#include <gtest/gtest.h>
#include <rt/rt2.h>
#include <numeric>

namespace ir::lang {
namespace {
void test_ir_build(std::string_view source, const std::vector<int64_t> &args, const int64_t expected_return) {
  scope s;
  ASSERT_NO_THROW(s = scope::parse(source));
#define target  "/home/luke/CLionProjects/compilers/bml/output"
  std::ofstream oasm;
  oasm.open(target ".asm");
  EXPECT_TRUE(oasm.is_open());
  oasm << R"(
section .text
global main
extern printf, malloc, exit


test_function:
)";

  ASSERT_NO_THROW(s.compile_as_function(oasm));
  oasm << "main:\n";
  oasm << "mov rdi, " << args.size() * 8 << "\n";
  oasm << "call malloc\n";
  oasm << "mov rdi, rax\n";
  for (int i = 0; i < args.size(); ++i) {
    oasm << "mov qword [rdi+" << i * 8 << "], " << rt::value::from_int(args[i]).v << "\n";
  }
  oasm << "call test_function\n";
  oasm << "ret\n";
  oasm.close();
  ASSERT_EQ(system("yasm -g dwarf2 -f elf64 " target ".asm -l " target ".lst -o " target ".o"), 0);
  ASSERT_EQ(system("gcc -no-pie " target ".o -o " target), 0);
  int64_t exit_code = rt::value::from_raw(WEXITSTATUS(system("timeout 1 " target " 2> " target ".stderr 1> " target ".stdout"))).to_int();
  //EXPECT_EQ(load_file(target ".stdout"), expected_stdout);
  //EXPECT_EQ(load_file(target ".stderr"), expected_stderr);
  EXPECT_EQ(exit_code, expected_return);
#undef target
}

}

TEST(Lang, IntMin) {
  std::string_view source = R"(
      x_v = argv[1];
      x = v_to_int(x_v);
      y_v = *argv;
      y = v_to_int(y_v);
      cmp (y, x);
      z = if (jle) then { return x; } else { return y; };
      z_v = int_to_v(z);
      return z_v;
)";
  test_ir_build(source, {42, 1729}, 42);
  test_ir_build(source, {45, 33}, 33);
}

TEST(Lang, IntMin_LastCall_PrevVar) {
  std::string_view source = R"(
      x_v = argv[1];
      x = v_to_int(x_v);
      y_v = *argv;
      y = v_to_int(y_v);
      cmp (y, x);
      z_v = if (jle) then {
        return x_v;
      } else {
        return y_v;
      };
      return z_v;
)";
  test_ir_build(source, {42, 1729}, 42);
  test_ir_build(source, {45, 33}, 33);
}

TEST(Lang, IntMin_LastCall_NewVar) {
  std::string_view source = R"(
      x_v = argv[1];
      x = v_to_int(x_v);
      y_v = *argv;
      y = v_to_int(y_v);
      cmp (y, x);
      z_v = if (jle) then {
        x_v_new = int_to_v(x);
        return x_v_new;
      } else {
        y_v_new = int_to_v(y);
        return y_v_new;
      };
      return z_v;
)";
  test_ir_build(source, {42, 1729}, 42);
  test_ir_build(source, {45, 33}, 33);
}

void sum_of_n_vars(const size_t n) {
  std::stringstream source;
  for (int i = 0; i < n; ++i) source << "x_v" << i << " = argv[" << i << "];\n";
  for (int i = 0; i < n; ++i) source << "x" << i << " = v_to_int(x_v" << i << ");\n";

  source << "sum0 = 0;\n";
  for (int i = 0; i < n; ++i) source << "sum" << (i + 1) << " = add(sum" << i << ", x" << i << ");\n";
  source << "ans_v = int_to_v(sum" << n << ");\n";
  source << "return ans_v;\n";

  //std::cout << source.str() << std::endl;

  std::vector<int64_t> args(n);
  std::iota(args.begin(), args.end(), 1);

  int64_t ans = std::accumulate(args.begin(), args.end(), 0);

  test_ir_build(source.str(), args, ans);
}

TEST(Build, SumZeroVars) { sum_of_n_vars(0); }
TEST(Build, SumOneVars) { sum_of_n_vars(1); }
TEST(Build, SumTwoVars) { sum_of_n_vars(2); }
TEST(Build, SumTenVars) { sum_of_n_vars(10); }
TEST(Build, SumTwentyVars) { sum_of_n_vars(20); }
TEST(Build, SumThirtyVars) { sum_of_n_vars(20); }

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
  /* std::cout << "-----------" << std::endl;

   s.print(std::cout);
   std::cout << "-----------" << std::endl;

   s.compile_as_function(std::cout);*/
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
  /* std::cout << "-----------" << std::endl;

   s.print(std::cout);
   std::cout << "-----------" << std::endl;

   s.compile_as_function(std::cout);*/
}
/*
TEST(Build, IntMin) {
  scope s;
  var x_sh("x_sh"), x("x"), next_arg("next_arg"), y_sh("y_sh"), y("y"), z("z");
  std::cout << "x:" << x.id << " y:" << y.id << std::endl;
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
  std::cout << "x:" << x.id << " y:" << y.id << std::endl;
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
  var p1("plus");
  var one("one");
  s << (one.assign(1));
  s << (p1.assign(rhs_expr::binary_op{.op = rhs_expr::binary_op::add, .x1 = z, .x2 = one}));
  s.ret = p1;
  std::cout << "-----------" << std::endl;

  s.print(std::cout);
  std::cout << "-----------" << std::endl;

  s.compile_as_function(std::cout);
}
*/
}
