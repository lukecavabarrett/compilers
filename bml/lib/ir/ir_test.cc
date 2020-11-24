#include <ir/lang.h>
#include <ir/ir.h>
#include <gtest/gtest.h>
#include <rt/rt2.h>
#include <numeric>
#include <charconv>
//TODO: improve test structure
// - pass parameters as a aggregate-initialized struct
// - use optionals if you don't want to execute the comparison (e.g. with stdout)
// - fix format of ALL tests
// - fix testing pipeline (creation of object files included)
// - add destruction in compilation
namespace ir::lang {
namespace {

int64_t remove_last_line(std::string &ss) {
  std::string_view s = ss;
  size_t last_nl = std::distance(s.rbegin(), std::find(s.rbegin(), s.rend(), 10));
  int64_t x = std::stoll(std::string(s.end() - last_nl - 1, last_nl + 1));
  ss.resize(ss.size() - last_nl - 1);
  return x;
}

std::pair<std::string_view, std::string_view> front_tail(std::string_view s) {
  auto it = std::find(s.rbegin(), s.rend(), 10);
  size_t last_line_size = std::distance(s.rbegin(), it);
  std::string_view tail(s.end() - last_line_size, last_line_size);
  s.remove_suffix(last_line_size);
  return {s, tail};
}

namespace build_object {
struct value;
typedef std::vector<value> tuple;

struct value_uptr : public std::shared_ptr<value> {
  typedef std::shared_ptr<value> base;
  using base::base;
  value_uptr(value &&);
  template<typename T>
  value_uptr(T &&t) : value_uptr(value(std::move(t))) {}
  template<typename T>
  value_uptr(const T &t) : value_uptr(value(t)) {}
};

struct tvar {
  uint64_t tag;
  value_uptr v;
};

struct fun {
  const std::string_view loc;
  const size_t n_params;
  fun(std::string_view loc, size_t n) : loc(loc), n_params(n) {}
  fun(const char *loc, size_t n) : loc(loc), n_params(n) {}
};

struct value {
  value(value &&) = default;
  value(const value &) = default;
  typedef std::variant<uint64_t, fun, tuple, tvar> variant;

  template<typename T>
  value(const T &t) : x(t) {}

  template<typename T>
  value(T &&t) : x(std::move(t)) {}

  value(int64_t i) : x(uint64_t(i)) {}

  template<typename T>
  value &operator=(const T &t) {
    x = t;
    return *this;
  }

  template<typename T>
  value &operator=(T &&t) {
    x = std::move(t);
    return *this;
  }

  value(std::initializer_list<value> &&l) : x(tuple(std::move(l))) {}
  value &at(size_t idx) {
    return std::get<tuple>(x).at(idx);
  }
  bool is_tuple() const {
    return std::holds_alternative<tuple>(x);
  }
  bool declared = false;
  size_t id;
  void clear_declaration() {
    declared = false;
    if (std::holds_alternative<tuple>(x)) {
      for (value &y : std::get<tuple>(x))y.clear_declaration();
    } else if (std::holds_alternative<tvar>(x)) {
      value *ptr = std::get<tvar>(x).v.get();
      if (ptr)ptr->clear_declaration();
    }
  }
#define Tag_Tuple  0
#define Tag_Fun 1
#define Tag_Arg 2

  static uint64_t make_tag_size_d(uint32_t tag, uint32_t size, uint8_t d) {
    return (((uint64_t) tag) << 32) | (((uint64_t) size) << 1) | (d & 1);
  }

  uintptr_t uint_to_v(uint64_t x) {
    return (uintptr_t) ((x << 1) | 1);
  }

  void declare(std::ostream &os) {
    using namespace util;
    if (declared)return;
    std::visit(overloaded{
        [](int64_t) {},
        [&](fun &f) {
          static size_t fun_blk_id = 0;
          id = fun_blk_id++;
          os << "__fun_block_" << id << "__ dq 0, " << make_tag_size_d(Tag_Fun, 2, 0) << ", " << f.loc << ", "
             << uint_to_v(f.n_params) << "\n";

        },
        [&](tuple &t) {
          static size_t tup_blk_id = 0;
          for (value &v : t)v.declare(os);
          id = tup_blk_id++;
          os << "__tuple_" << id << "__ dq 0, " << make_tag_size_d(Tag_Tuple, t.size(), 0);
          for (value &v : t) {
            os << ", ";
            v.retrieve(os);
          }
          os << "\n";
        },
        [&](tvar &tv) {
          if (tv.v) {
            tv.v->declare(os);
            static size_t tvar_blk_id = 0;
            id = tvar_blk_id++;
            os << "__variant_block_" << id << "__ dq 0, " << make_tag_size_d(tv.tag, 1, 0) << ", ";
            tv.v->retrieve(os);
            os << "\n";
          }
        }
    }, x);
    declared = true;
  }
  void retrieve(std::ostream &os) const {
    assert(declared);
    std::visit(util::overloaded{
        [&](int64_t n) {
          os << (2 * n + 1);
        },
        [&](const fun &f) {
          os << "__fun_block_" << id << "__";
        },
        [&](const tuple &t) {
          os << "__tuple_" << id << "__";
        },
        [&](const tvar &tv) {
          if (tv.v)
            os << "__variant_block_" << id << "__";
          else os << tv.tag;
        }
    }, x);
  }
  variant x;
};

value_uptr::value_uptr(value &&v) {
  reset(std::make_unique<value>(std::move(v)).release());
}

std::ostream &operator<<(std::ostream &os, const value &v) {
  std::visit(util::overloaded{
      [&](int64_t n) {
        os << "[" << n << "]";
      },
      [&](const fun &f) {
        os << "{rc:static, tag:Fun, size:2 | [text_ptr @" << f.loc << "][" << f.n_params << "]}";
      },
      [&](const tuple &t) {
        os << "{rc:static, tag:Tuple, size:" << t.size() << " | ";
        for (const value &v : t)os << v;
        os << "}";
      },
      [&](const tvar &tv) {
        if (tv.v) {
          os << "{rc:static, tag:" << tv.tag << ", size:" << 1 << " | ";
          os << (*tv.v);
          os << "}";
        } else {
          os << "[tag: " << tv.tag << "]";
        }
      }
  }, v.x);
  return os;
}

TEST(BuildObject, Syntax) {
  value f1 = fun("some_fun", 2);
  value f2 = fun("some_fun", 2);
  value n1 = 34;
  value t0 = {};
  EXPECT_TRUE(t0.is_tuple());
  value t1 = {1};
  EXPECT_TRUE(t1.is_tuple());

  value t3 = {1, 2, fun("gianni", 1)};
  EXPECT_TRUE(t3.is_tuple());

  value t4 = {3, {}, {1}, {fun("jack", 7)}};
  EXPECT_TRUE(t4.is_tuple());
  EXPECT_TRUE(t4.at(1).is_tuple());
  EXPECT_TRUE(t4.at(2).is_tuple());
  EXPECT_TRUE(t4.at(3).is_tuple());

  value none = tvar{.tag = 3};
  EXPECT_EQ(std::get<tvar>(none.x).v, nullptr);
  value some_3 = tvar{.tag = 5, .v = 3};

}

}
namespace out_object {
struct value {
  struct text_ptr {};
  struct tuple {
    uint32_t tag;
    std::vector<value> vs;
  };
  typedef uint64_t imm;
  typedef std::variant<text_ptr, tuple, imm> variant;
  variant x;
  template<typename T>
  value(const T &t):x(t) {}
  template<typename T>
  value(T &&t):x(std::move(t)) {}
  bool operator==(const build_object::value &bo) const {
    //bo std::variant<uint64_t, fun, tuple, tvar>
    return std::visit(util::overloaded{
      [](text_ptr)->bool{
        return false;
        },
      [&](const tuple& t)->bool{
        if(t.tag == Tag_Tuple) {
          if (!std::holds_alternative<build_object::tuple>(bo.x))return false;
          const build_object::tuple &t2 = std::get<build_object::tuple>(bo.x);
          return std::equal(t.vs.begin(),t.vs.end(),t2.begin(),t2.end());
        } else if(t.tag == Tag_Fun){
          if (!std::holds_alternative<build_object::fun>(bo.x))return false;
          const build_object::fun &f = std::get<build_object::fun>(bo.x);
          if(t.vs.size()!=2)return false;
          if(!std::holds_alternative<text_ptr>(t.vs.at(0).x))return false;
          if(!std::holds_alternative<uint64_t>(t.vs.at(1).x))return false;
          uint64_t p = std::get<uint64_t>(t.vs.at(1).x);
          return p == f.n_params;
        }else{
          EXPECT_LT(t.vs.size(),2);
          if (!std::holds_alternative<build_object::tvar>(bo.x))return false;
          const build_object::tvar &t2 = std::get<build_object::tvar>(bo.x);
          if(t.tag != t2.tag)return false;
          if(bool(!t.vs.empty()) != bool(t2.v))return false;
          if(!t.vs.empty())return t.vs.at(0) == *t2.v;
          return true;
        }
      },
      [&](uint64_t v)->bool{
        return std::holds_alternative<uint64_t>(bo.x) && std::get<uint64_t>(bo.x)==v;
      }
      },x);
  }

};
std::ostream &operator<<(std::ostream &os, const value &v) {
  std::visit(util::overloaded{
      [&](uint64_t n) { os << "[" << n << "]"; },
      [&](const value::tuple &v) {
        os << "{rc:?, tag:"<<v.tag<<", size:"<<v.vs.size()<<" |" ;
        for(const value& x : v.vs)os<<x;
        os << "}";
      },
      [&](value::text_ptr) { os << "[text_ptr]"; }
  }, v.x);
  return os;
}
value parse(std::string_view &s) {
  assert(!s.empty());
  if (s.front() == '[') {
    static constexpr std::string_view text_ptr_str = "[text_ptr]";
    if(s.starts_with(text_ptr_str)){
      s.remove_prefix(text_ptr_str.size());
      return value::text_ptr{};
    }
    int read;
    uint64_t v;
    EXPECT_EQ(sscanf(s.data(), "[%lu]%n", &v, &read), 1);
    s.remove_prefix(read);
    return value(v);
  } else if (s.front() == '{') {
    int read;
    uint32_t tag,size;
    EXPECT_EQ(sscanf(s.data(), "{rc:%*[^,], tag:%u, size:%u | %n", &tag,&size, &read), 2);
    s.remove_prefix(read);
    value::tuple t{.tag=tag};
    while (!s.empty() && s.front() != '}')
      t.vs.push_back(parse(s));
    s.remove_prefix(1);
    EXPECT_EQ(t.vs.size(),size);
    return t;
  } else {
    char c = s.front();
    THROW_INTERNAL_ERROR
  }
}
}

void test_ir_build(std::string_view source,
                   build_object::value &&arg,
                   build_object::value expected_return,
                   std::string_view expected_stdout = "",
                   std::string_view expected_stderr = "") {

#define target  "/home/luke/CLionProjects/compilers/bml/output"
  std::ofstream oasm;
  oasm.open(target ".asm");
  EXPECT_TRUE(oasm.is_open());
  const auto &info = *::testing::UnitTest::GetInstance()->current_test_info();
  oasm << "; TEST " << info.test_suite_name() << " > " << info.test_case_name() << " > " << info.name() << "\n";
  oasm << R"(
section .data
retcode_format db  10,"%llu", 0
)";
  arg.clear_declaration();
  arg.declare(oasm);

  oasm << R"(
section .text
global main
extern printf, malloc, exit, print_debug, sum_fun, apply_fn

)";

  parse::tokenizer tk(source);
  while (!tk.empty()) {
    function f;
    ASSERT_NO_THROW(f.parse(tk));
    f.compile(oasm);
  }

  oasm << "main:\n";
  oasm << "push rdi\n";
  oasm << "mov rdi, ";
  arg.retrieve(oasm);
  oasm << R"(
call test_function
mov     rdi, rax
call    print_debug
pop     rdi
xor     eax, eax
ret
)";
  oasm.close();
  ASSERT_EQ(system("yasm -g dwarf2 -f elf64 " target ".asm -l " target ".lst -o " target ".o"), 0);
  ASSERT_EQ(system("gcc -no-pie " target ".o /home/luke/CLionProjects/compilers/bml/lib/rt/rt.o -o " target), 0);
  int exit_code = (WEXITSTATUS(system("timeout 1 " target " 2> " target ".stderr 1> " target ".stdout")));
  ASSERT_EQ(exit_code, 0);
  std::string actual_stdout = util::load_file(target ".stdout");
  std::string whole_stderr = util::load_file(target ".stderr");
  auto[actual_stderr, returned_stderr] = front_tail(whole_stderr);
  //int64_t actual_return = remove_last_line(actual_stdout);
  out_object::value actual_return = out_object::parse(returned_stderr);
  EXPECT_TRUE(returned_stderr.empty());
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
  test_function(argv : non_trivial) {
      x_v : unboxed = argv[2];
      x = v_to_int(x_v);
      y_v = argv[3];
      y = v_to_int(y_v);
      cmp (y, x);
      z = if (jle) then { return x; } else { return y; };
      z_v = int_to_v(z);
      return z_v;
  }
)";
  // test_ir_build(source, {42, 1729}, 42);
  //test_ir_build(source, {45, 33}, 33);
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
  //test_ir_build(source, {42, 1729}, 42);
  // test_ir_build(source, {45, 33}, 33);
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
  //test_ir_build(source, {42, 1729}, 42);
  //test_ir_build(source, {45, 33}, 33);
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
  // test_ir_build(source, {42, 1729}, 0);
  // test_ir_build(source, {45, 33}, 1);
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
  // test_ir_build(source, {42, 1729}, 0);
  // test_ir_build(source, {45, 33}, 1);
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

//  build_object::tuple args(n);
  // std::iota(args.begin(), args.end(), 1);

//  int64_t ans = n*(n+1)/2;

  // // test_ir_build(source.str(), build_object::value(std::move(args)), ans);
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
  // test_ir_build(source, {}, 42);
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
  // test_ir_build(source, {42}, 42);
}
using build_object::fun;
TEST(Memory, MakeTuple) {
  std::string_view source = R"(
test_function(num) {
block = malloc(5);
zero = 0;
block[0] := zero;
tag_size_d = 6;
block[1] := tag_size_d;
block[2] := num;
m1729 = 1729;
m1729_v = int_to_v(m1729);
block[3] := m1729_v;
m42 = 42;
m42_v = int_to_v(m42);
block[4] := m42_v;
return block;
}
)";
  test_ir_build(source, fun("test_function",1), {fun("test_function",1),1729,42});
}

TEST(Memory, CallApplyFn) {
  std::string_view source = R"(
test_function(args) {
f = args[2];
x1 = args[3];
x2 = args[4];
y1 = apply_fn(f,x1);
y2 = apply_fn(y1,x2);
return y2;
}
)";
  function::parse(source).print(std::cout);
  test_ir_build(source, {fun("sum_fun",2),100,10}, 110,"","destroying block of size 3\ndestroying block of size 3\n");
}


TEST(Memory, DestroyABlock) {

}

}
