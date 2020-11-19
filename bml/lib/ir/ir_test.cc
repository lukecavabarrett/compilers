#include <ir/lang.h>
#include <ir/ir.h>
#include <gtest/gtest.h>
#include <rt/rt2.h>
#include <numeric>
#include <charconv>

namespace ir::lang {
namespace {
void test_ir_build(std::string_view source, const std::vector<int64_t> &args, const int64_t expected_return) {
  scope s;
  ASSERT_NO_THROW(s = scope::parse(source));
#define target  "/home/luke/CLionProjects/compilers/bml/output"
  std::ofstream oasm;
  oasm.open(target ".asm");
  EXPECT_TRUE(oasm.is_open());
  const auto &info = *::testing::UnitTest::GetInstance()->current_test_info();
  oasm << "; TEST " << info.test_suite_name() << " > " << info.test_case_name() << " > " << info.name() << "\n";
  oasm << R"(
section .data
uint64_format db  "%llu", 0
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

  oasm << R"(
call test_function
mov     rsi, rax
xor     eax, eax
mov     edi, uint64_format
call    printf
xor     eax, eax
ret
)";
  oasm << "call test_function\n";
  oasm << "ret\n";
  oasm.close();
  ASSERT_EQ(system("yasm -g dwarf2 -f elf64 " target ".asm -l " target ".lst -o " target ".o"), 0);
  ASSERT_EQ(system("gcc -no-pie " target ".o -o " target), 0);
  int exit_code = (WEXITSTATUS(system("timeout 1 " target " 2> " target ".stderr 1> " target ".stdout")));
  EXPECT_EQ(exit_code, 0);
  int64_t actual_return = rt::value::from_raw(std::stoll(util::load_file(target ".stdout"))).to_int();
  //EXPECT_EQ(util::load_file(target ".stdout"), expected_stdout);
  //EXPECT_EQ(load_file(target ".stderr"), expected_stderr);
  EXPECT_EQ(actual_return, expected_return);
#undef target
}

}

TEST(RegLru, CopyConstr) {
  ir::reg_lru rl1;

  register_t r = rl1.front();
  rl1.bring_back(r);

  ir::reg_lru rl2(rl1);
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
  std::string_view source = R"(
fortytwo = 42;
a = int_to_v(fortytwo);
b = a;
c = b;
d = c;
e = d;
return e;
)";
  test_ir_build(source, {}, 42);
}

TEST(Build, PassingVar) {
  std::string_view source = R"(
a = *argv;
b = a;
c = b;
d = c;
e = d;
return e;
)";
  test_ir_build(source, {42}, 42);
}

TEST(Build, MakeTuple) {
  std::string_view source = R"(
block = malloc(10);
val = *argv;
block[7] := val;
x = block[7];
return x;
)";
  test_ir_build(source, {42}, 42);
}

}
