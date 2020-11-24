#include <ir/lang.h>
#include <ir/ir.h>
#include <gtest/gtest.h>
#include <rt/rt2.h>
#include <numeric>
#include <charconv>

namespace ir::lang {
namespace {

int64_t remove_last_line(std::string &ss) {
  std::string_view s = ss;
  size_t last_nl = std::distance(s.rbegin(), std::find(s.rbegin(), s.rend(), 10));
  int64_t x = std::stoll(std::string(s.end() - last_nl - 1, last_nl + 1));
  ss.resize(ss.size() - last_nl - 1);
  return x;
}

void test_ir_build(std::string_view source, const std::vector<int64_t> &args, const int64_t expected_return, std::string_view expected_stdout = "", std::string_view expected_stderr = "") {

#define target  "/home/luke/CLionProjects/compilers/bml/output"
  std::ofstream oasm;
  oasm.open(target ".asm");
  EXPECT_TRUE(oasm.is_open());
  const auto &info = *::testing::UnitTest::GetInstance()->current_test_info();
  oasm << "; TEST " << info.test_suite_name() << " > " << info.test_case_name() << " > " << info.name() << "\n";
  oasm << R"(
section .data
retcode_format db  10,"%llu", 0
section .text
global main
extern printf, malloc, exit

)";

  parse::tokenizer tk(source);
  while (!tk.empty()) {
    function f;
    ASSERT_NO_THROW(f.parse(tk));
    f.compile(oasm);
  }

  oasm << "main:\n";
  oasm << "mov rdi, " << (args.size() + 2) * 8 << "\n";
  oasm << "call malloc\n";
  oasm << "mov rdi, rax\n";
  oasm << "mov qword [rdi], 0\n"; // indestructible
  oasm << "mov qword [rdi+8], " << (args.size() * 2) << "\n"; // indestructible
  for (int i = 0; i < args.size(); ++i) {
    oasm << "mov qword [rdi+" << (i + 2) * 8 << "], " << rt::value::from_int(args[i]).v << "\n";
  }

  oasm << R"(
call test_function
mov     rsi, rax
sar     rsi, 1
xor     eax, eax
mov     edi, retcode_format
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
  std::string actual_stdout = util::load_file(target ".stdout"), actual_stderr = util::load_file(target ".stderr");
  int64_t actual_return = remove_last_line(actual_stdout);
  EXPECT_EQ(actual_return, expected_return);
  EXPECT_EQ(actual_stdout, expected_stdout);
  EXPECT_EQ(actual_stderr, expected_stderr);
#undef target

}

}

TEST(RegLru, CopyConstr) {
  ir::reg_lru rl1;

  register_t r = rl1.front();
  rl1.bring_back(r);

  ir::reg_lru rl2(rl1);
}

TEST(Build, IntMin) {
  std::string_view source = R"(
  test_function(argv) {
      x_v : imm = argv[2];
      x = v_to_int(x_v);
      y_v = argv[3];
      y = v_to_int(y_v);
      cmp (y, x);
      z = if (jle) then { return x; } else { return y; };
      z_v = int_to_v(z);
      return z_v;
  }
)";
  test_ir_build(source, {42, 1729}, 42);
  test_ir_build(source, {45, 33}, 33);
}

TEST(Build, IntMin_LastCall_PrevVar) {
  std::string_view source = R"(
  test_function(argv) {
      x_v = argv[2];
      x = v_to_int(x_v);
      y_v = argv[3];
      y = v_to_int(y_v);
      cmp (y, x);
      z_v = if (jle) then {
        return x_v;
      } else {
        return y_v;
      };
      return z_v;
  }
)";
  test_ir_build(source, {42, 1729}, 42);
  test_ir_build(source, {45, 33}, 33);
}

TEST(Build, IntMin_LastCall_NewVar_inside) {
  std::string_view source = R"(
  test_function(argv) {
      x_v = argv[2];
      x = v_to_int(x_v);
      y_v = argv[3];
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
  }
)";
  test_ir_build(source, {42, 1729}, 42);
  test_ir_build(source, {45, 33}, 33);
}

TEST(Build, ArgMin) {
  std::string_view source = R"(
  test_function(argv) {
      x_v = argv[2];
      x = v_to_int(x_v);
      y_v = argv[3];
      y = v_to_int(y_v);
      cmp (y, x);
      z_v = if (jle) then {
        x_id = 0;
        x_id_v = int_to_v(x_id);
        return x_id_v;
      } else {
        y_id = 1;
        y_id_v = int_to_v(y_id);
        return y_id_v;
      };
      return z_v;
  }
)";
  test_ir_build(source, {42, 1729}, 0);
  test_ir_build(source, {45, 33}, 1);
}

TEST(Build, ArgMin_ConvertOutside) {
  std::string_view source = R"(
  test_function(argv) {
      x_v = argv[2];
      x = v_to_int(x_v);
      y_v = argv[3];
      y = v_to_int(y_v);
      cmp (y, x);
      z = if (jle) then {
        x_id = 0;
        return x_id;
      } else {
        y_id = 1;
        return y_id;
      };
      z_v = int_to_v(z);
      return z_v;
  }
)";
  test_ir_build(source, {42, 1729}, 0);
  test_ir_build(source, {45, 33}, 1);
}

void sum_of_n_vars(const size_t n) {
  std::stringstream source;
  source << "test_function (argv) { \n";
  for (int i = 0; i < n; ++i) source << "x_v" << i << " = argv[" << (i + 2) << "];\n";
  for (int i = 0; i < n; ++i) source << "x" << i << " = v_to_int(x_v" << i << ");\n";
  source << "sum0 = 0;\n";
  for (int i = 0; i < n; ++i) source << "sum" << (i + 1) << " = add(sum" << i << ", x" << i << ");\n";
  source << "ans_v = int_to_v(sum" << n << ");\n";
  source << "return ans_v;\n";
  source << "} \n";
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
test_function() {
  fortytwo = 42;
  a = int_to_v(fortytwo);
  b = a;
  c = b;
  d = c;
  e = d;
  return e;
}
)";
  test_ir_build(source, {}, 42);
}

TEST(Build, PassingVar) {
  std::string_view source = R"(
test_function(argv) {
a = argv[2];
b = a;
c = b;
d = c;
e = d;
return e;
}
)";
  test_ir_build(source, {42}, 42);
}

TEST(Memory, MakeTuple) {
  std::string_view source = R"(
test_function(argv) {
block = malloc(10);
val = argv[2];
block[7] := val;
x = block[7];
return x;
}
)";
  test_ir_build(source, {42}, 42);
}

TEST(Memory, DestroyABlock) {

}

}
