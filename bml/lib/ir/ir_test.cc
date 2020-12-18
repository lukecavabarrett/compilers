#include <ir/lang.h>
#include <ir/ir.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <rt/rt2.h>
#include <numeric>
#include <charconv>
#include <random>
//TODO: improve test structure
// - pass parameters as a aggregate-initialized struct
// - use optionals if you don't want to execute the comparison (e.g. with stdout)
// - fix format of ALL tests
// - fix testing pipeline (creation of object files included)
// - add destruction in compilation
// - make sure memory_access is incrementing the refcount
// - delay destruction after comparison

// TODO: optional: is there a way to override the ostream for doing (for instance) putting ";" before evry line?
//TODO-someday: understand why pushing and retrieving {rdi} is okay, but not {} or (rdi,rsi}
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
  tvar(uint64_t tag) : tag(tag) {}
  tvar(uint64_t tag, value &&v) : tag(tag), v(std::move(v)) {}
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

  void allocate_dynamically(std::ostream &os) const {
    using namespace util;
    std::visit(overloaded{
        [&](int64_t n) {
          os << "push " << (2 * n + 1) << "\n";
        },
        [&](const fun &f) {
          os << "mov " << reg::to_string(reg::args_order.front()) << ", " << (4 * 8) << "\n";
          os << "call malloc\n";
          os << "mov qword [rax], 3\n";
          os << "mov rbx, " << make_tag_size_d(Tag_Fun, 2, 0) << "\n";
          os << "mov qword [rax+8], rbx\n";
          os << "mov qword [rax+16], " << f.loc << "\n";
          os << "mov qword [rax+24], " << f.n_params << "\n";
          os << "push rax\n";

        },
        [&](const tuple &t) {
          for (auto vit = t.rbegin(); vit != t.rend(); ++vit)vit->allocate_dynamically(os);
          os << "mov " << reg::to_string(reg::args_order.front()) << ", " << ((t.size() + 2) * 8) << "\n";
          os << "call malloc\n";
          os << "mov qword [rax], 3\n";
          os << "mov rbx, " << make_tag_size_d(Tag_Tuple, t.size(), 0) << "\n";
          os << "mov qword [rax+8], rbx\n";
          for (size_t i = 0; i < t.size(); ++i) {
            os << "pop rbx\n";
            os << "mov qword [rax+" << ((i + 2) * 8) << "], rbx\n";
          }
          os << "push rax\n";
        },
        [&](const tvar &tv) {
          if (tv.v) {
            tv.v->allocate_dynamically(os);
            os << "mov " << reg::to_string(reg::args_order.front()) << ", " << (3 * 8) << "\n";
            os << "call malloc\n";
            os << "mov qword [rax], 3\n";
            os << "mov rbx, " << make_tag_size_d(tv.tag, 1, 0) << "\n";
            os << "mov qword [rax+8], rbx\n";
            os << "pop rbx\n";
            os << "mov qword [rax+16], rbx\n";
            os << "push rax\n";
          } else {
            os << "push " << tv.tag << "\n";
          }
        }
    }, x);
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

  value none = tvar(3);
  EXPECT_EQ(std::get<tvar>(none.x).v, nullptr);
  value some_3 = tvar(5, 3);

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
        [](text_ptr) -> bool {
          return false;
        },
        [&](const tuple &t) -> bool {
          if (t.tag == Tag_Tuple) {
            if (!std::holds_alternative<build_object::tuple>(bo.x))return false;
            const build_object::tuple &t2 = std::get<build_object::tuple>(bo.x);
            return std::equal(t.vs.begin(), t.vs.end(), t2.begin(), t2.end());
          } else if (t.tag == Tag_Fun) {
            if (!std::holds_alternative<build_object::fun>(bo.x))return false;
            const build_object::fun &f = std::get<build_object::fun>(bo.x);
            if (t.vs.size() != 2)return false;
            if (!std::holds_alternative<text_ptr>(t.vs.at(0).x))return false;
            if (!std::holds_alternative<uint64_t>(t.vs.at(1).x))return false;
            uint64_t p = std::get<uint64_t>(t.vs.at(1).x);
            return p == f.n_params;
          } else {
            EXPECT_LT(t.vs.size(), 2);
            if (!std::holds_alternative<build_object::tvar>(bo.x))return false;
            const build_object::tvar &t2 = std::get<build_object::tvar>(bo.x);
            if (t.tag != t2.tag)return false;
            if (bool(!t.vs.empty()) != bool(t2.v))return false;
            if (!t.vs.empty())return t.vs.at(0) == *t2.v;
            return true;
          }
        },
        [&](uint64_t v) -> bool {
          return std::holds_alternative<uint64_t>(bo.x) && std::get<uint64_t>(bo.x) == v;
        }
    }, x);
  }

};
std::ostream &operator<<(std::ostream &os, const value &v) {
  std::visit(util::overloaded{
      [&](uint64_t n) { os << "[" << n << "]"; },
      [&](const value::tuple &v) {
        os << "{rc:?, tag:" << v.tag << ", size:" << v.vs.size() << " |";
        for (const value &x : v.vs)os << x;
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
    if (s.starts_with(text_ptr_str)) {
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
    uint32_t tag, size;
    EXPECT_EQ(sscanf(s.data(), "{rc:%*[^,], tag:%u, size:%u | %n", &tag, &size, &read), 2);
    s.remove_prefix(read);
    value::tuple t{.tag=tag};
    while (!s.empty() && s.front() != '}')
      t.vs.push_back(parse(s));
    s.remove_prefix(1);
    EXPECT_EQ(t.vs.size(), size);
    return t;
  } else {
    char c = s.front();
    THROW_INTERNAL_ERROR
  }
}
}

enum call_style_t { as_args, as_tuple, progressive_application };
struct testcase_params {
  std::string_view test_function = "test_function";
  bool code_verbose = false;
  bool allocate_input_dynamically = false;
  call_style_t call_style = as_tuple;
  std::unordered_map<std::string_view, size_t> curriables = {};
  int expected_exit_code = 0;
  build_object::value expected_return = 42;
  bool debug_log = false;
  std::variant<std::monostate, std::string_view, ::testing::PolymorphicMatcher<testing::internal::MatchesRegexMatcher> >
      expected_stdout, expected_stderr;
};

void test_ir_build(std::string_view source,
                   build_object::value arg,
                   testcase_params params) {

  parse::tokenizer tk(source);
  std::vector<function> fs;
  while (!tk.empty()) {
    function f;
    ASSERT_NO_THROW(f.parse(tk));
    f.setup_destruction();
    if (params.code_verbose)f.print(std::cout);
    fs.push_back(std::move(f));
  }
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

  if (!params.allocate_input_dynamically) {
    arg.clear_declaration();
    arg.declare(oasm);
  }

  for (const auto &[f, n] : params.curriables) {
    oasm << "__fun_block_" << f << "__ dq 0, 4294967300, " << f << ", " << (2 * n + 1) << "\n";
  }

  oasm << R"(
section .text
global main
extern printf, malloc, exit, print_debug, sum_fun, apply_fn, decrement_nontrivial, decrement_value, increment_value, println_int, println_int_err,println_int_err_skim

)";

  for (auto &f : fs)f.compile(oasm);

  oasm << "main:\n";
  oasm << "push rdi\n";
  //load args
  switch (params.call_style) {

    case as_args: {
      if (!std::holds_alternative<build_object::tuple>(arg.x))
        FAIL() << "mode \"as_args\" was selected, but the params were not a tuple/list.\n";
      auto it = reg::args_order.begin();
      const auto &arg_tuple = std::get<build_object::tuple>(arg.x);
      if (params.allocate_input_dynamically)
        std::for_each(arg_tuple.rbegin(), arg_tuple.rend(), [&](const auto &v) {
          v.allocate_dynamically(oasm);
        });
      for (const auto &p : arg_tuple) {
        if (it == reg::args_order.end())
          FAIL() << "specified arguments exceeded the number of parameter-specific registers\n";
        if (params.allocate_input_dynamically) {
          oasm << "pop " << reg::to_string(*it) << "\n";
        } else {
          oasm << "mov " << reg::to_string(*it) << ", ";
          p.retrieve(oasm);
          oasm << "\n";
        }
        ++it;
      }
      oasm << "call " << params.test_function << "\n";
      break;
    };
    case as_tuple: {
      if (params.allocate_input_dynamically) {
        arg.allocate_dynamically(oasm);
        oasm << "pop rdi\n";
      } else {
        oasm << "mov rdi, ";
        arg.retrieve(oasm);
        oasm << "\n";
      }
      oasm << "call " << params.test_function << "\n";
      break;
    };
    case progressive_application: {

      if (!std::holds_alternative<build_object::tuple>(arg.x))
        FAIL() << "mode \"progressive_application\" was selected, but the params were not a tuple/list.\n";

      if (!params.curriables.contains(params.test_function))
        FAIL() << "mode \"progressive_application\" was selected, but the selected entry point ("
               << params.test_function << ") was not specified as a curriable.\n";

      const auto &arg_tuple = std::get<build_object::tuple>(arg.x);
      if (params.allocate_input_dynamically)
        std::for_each(arg_tuple.rbegin(), arg_tuple.rend(), [&](const auto &v) {
          v.allocate_dynamically(oasm);
        });

      oasm << "mov rax, __fun_block_" << params.test_function << "__\n";
      for (const auto &p : std::get<build_object::tuple>(arg.x)) {
        oasm << "mov rdi, rax\n";
        if (params.allocate_input_dynamically) {
          oasm << "pop rsi\n";
        } else {
          oasm << "mov rsi, ";
          p.retrieve(oasm);
          oasm << "\n";
        }

        oasm << "call apply_fn\n";
      }
      break;
    };
  }
  //result in rax

  oasm << R"(
mov     rdi, rax
call    print_debug
pop     rdi
xor     eax, eax
ret
)";
  oasm.close();
  ASSERT_EQ(system(params.debug_log
                   ? "gcc -c /home/luke/CLionProjects/compilers/bml/lib/rt/rt.c -o /home/luke/CLionProjects/compilers/bml/lib/rt/rt.o -g -O0 -DDEBUG_LOG"
                   :
                   "gcc -c /home/luke/CLionProjects/compilers/bml/lib/rt/rt.c -o /home/luke/CLionProjects/compilers/bml/lib/rt/rt.o -g -O0 "),
            0);
  ASSERT_EQ(system("yasm -g dwarf2 -f elf64 " target ".asm -l " target ".lst -o " target ".o"), 0);
  ASSERT_EQ(system("gcc -no-pie " target ".o /home/luke/CLionProjects/compilers/bml/lib/rt/rt.o -o " target), 0);
  int exit_code = (WEXITSTATUS(system("timeout 1 " target " 2> " target ".stderr 1> " target ".stdout")));
  ASSERT_EQ(exit_code, params.expected_exit_code);
  std::string actual_stdout = util::load_file(target ".stdout");
  std::string whole_stderr = util::load_file(target ".stderr");
  auto[actual_stderr, returned_stderr] = front_tail(whole_stderr);
  out_object::value actual_return = out_object::parse(returned_stderr);
  EXPECT_TRUE(returned_stderr.empty());
  EXPECT_EQ(actual_return, params.expected_return);
  std::visit(util::overloaded{
      [](std::monostate) {},
      [&](std::string_view expected_stdout) { EXPECT_EQ(actual_stdout, expected_stdout); },
      [&](const ::testing::PolymorphicMatcher<testing::internal::MatchesRegexMatcher> &expected_stdout) {
        EXPECT_THAT(actual_stdout, expected_stdout);
      }
  }, params.expected_stdout);
  std::visit(util::overloaded{
      [](std::monostate) {},
      [&actual_stderr = actual_stderr](std::string_view expected_stderr) { EXPECT_EQ(actual_stderr, expected_stderr); },
      [&actual_stderr =
      actual_stderr](const ::testing::PolymorphicMatcher<testing::internal::MatchesRegexMatcher> &expected_stderr) {
        EXPECT_THAT(actual_stderr, expected_stderr);
      }
  }, params.expected_stderr);

#undef target

}

}

using ::testing::MatchesRegex;

TEST(RegLru, CopyConstr) {
  ir::reg_lru rl1;

  register_t r = rl1.front();
  rl1.bring_back(r);

  ir::reg_lru rl2(rl1);
}

TEST(Build, UnboxTuple) {
  std::string_view source = R"(
  test_function(tuple : non_trivial) {
      x_v : trivial = tuple[2];
      return x_v;
  }
)";
  test_ir_build(source, {42}, {.allocate_input_dynamically=false, .expected_return = 42});
  test_ir_build(source, {42}, {.allocate_input_dynamically=true, .expected_return = 42});
}

TEST(Build, UnboxTupleOfTuple) {
  std::string_view source = R"(
  test_function(ttuple : non_trivial) {
      tuple : non_trivial = ttuple[2];
      x_v : trivial = tuple[2];
      return x_v;
  }
)";
  test_ir_build(source, {{42}}, {.allocate_input_dynamically=false, .expected_return = 42});
  test_ir_build(source, {{42}}, {.allocate_input_dynamically=true, .expected_return = 42});
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
  test_ir_build(source, {42, 1729}, {.expected_return = 42});
  test_ir_build(source, {45, 33}, {.expected_return = 33});
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
  test_ir_build(source, {42, 1729}, {.expected_return = 42});
  test_ir_build(source, {45, 33}, {.expected_return = 33});
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
  test_ir_build(source, {42, 1729}, {.expected_return = 42});
  test_ir_build(source, {45, 33}, {.expected_return = 33});
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

  test_ir_build(source, {42, 1729}, {.expected_return=0});
  test_ir_build(source, {45, 33}, {.expected_return=1});
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
  test_ir_build(source, {42, 1729}, {.expected_return=0});
  test_ir_build(source, {45, 33}, {.expected_return=1});
}

void sum_of_n_vars(const size_t n) {
  static std::mt19937 e2(1728);
  static std::uniform_int_distribution<int> uniform_dist(1, 1000000);

  std::stringstream source;
  source << "test_function (argv) { \n";
  for (int i = 0; i < n; ++i) source << "x_v" << i << " = argv[" << (i + 2) << "];\n";
  for (int i = 0; i < n; ++i) source << "x" << i << " = v_to_int(x_v" << i << ");\n";
  source << "sum0 = 0;\n";
  for (int i = 0; i < n; ++i) source << "sum" << (i + 1) << " = add(sum" << i << ", x" << i << ");\n";
  source << "ans_v = int_to_v(sum" << n << ");\n";
  source << "return ans_v;\n";
  source << "} \n";
  build_object::tuple args;
  uint64_t ans = 0;
  for (size_t i = 1; i <= n; ++i) {
    uint64_t x = uniform_dist(e2);
    ans += x;
    args.emplace_back(x);
  }

  test_ir_build(source.str(), build_object::value(std::move(args)), {.expected_return =  ans});
}

TEST(Build, SumZeroVars) { sum_of_n_vars(0); }
TEST(Build, SumOneVars) { sum_of_n_vars(1); }
TEST(Build, SumTwoVars) { sum_of_n_vars(2); }
TEST(Build, SumTenVars) { sum_of_n_vars(10); }
TEST(Build, SumTwentyVars) { sum_of_n_vars(20); }
TEST(Build, SumThirtyVars) { sum_of_n_vars(30); }
TEST(Build, SumFiftyVars) { sum_of_n_vars(50); }

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
  test_ir_build(source, {}, {.expected_return = 42});
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
  test_ir_build(source, {42}, {.call_style = as_tuple, .expected_return=42});
}

TEST(Build, PassingInput) {
  std::string_view source = R"(
test_function(x) {
a = x;
b = a;
c = b;
d = c;
e = d;
return e;
}
)";
  test_ir_build(source, {42}, {.call_style = as_args, .expected_return=42});
}

using build_object::fun;
TEST(Memory, MakeTuple) {
  std::string_view source = R"(
make_tuple(num) {
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
  test_ir_build(source,
                fun("make_tuple", 1),
                {.test_function="make_tuple",
                    .expected_return={fun("make_tuple", 1), 1729, 42}});
}

TEST(Memory, CallApplyFn) {
  // apply2 : ('a -> 'b -> 'c) -> 'a -> 'b -> 'c = <fun>
  std::string_view source = R"(
apply2(args) {
f = args[2];
x1 = args[3];
x2 = args[4];
y1 = apply_fn(f,x1);
y2 = apply_fn(y1,x2);
return y2;
}
)";
  test_ir_build(source,
                {fun("sum_fun", 2), 100, 10},
                {.test_function="apply2", .expected_return=110});
}

TEST(Recursive, ListLength) {
  using build_object::tvar;
  std::string_view source = R"(
length(args) {
  x = args[4];
  one = 1;

  one_v = int_to_v(one);

  test(x,one);
  ans_v = if (jne) then {
    this_f = args[2];
    cnt = x[2];
    tl = cnt[3];
    tl_len_v = apply_fn(this_f,tl);
    tl_len = v_to_int(tl_len_v);
    all_len = add(tl_len,one);
    all_len_v = int_to_v(all_len);
    return all_len_v;
  } else {
    zero = 0;
    zero_v = int_to_v(zero);
    return zero_v;
  };
  return ans_v;
}
)";
  testcase_params tp = {.test_function="length", .call_style=progressive_application, .curriables={{"length", 1}}};
  build_object::value list = tvar(3); //empty-list
  for (size_t l = 0; l < 10; ++l) {
    tp.expected_return = l;
    test_ir_build(source, {list}, tp);
    list = tvar(5, {l, std::move(list)});
  }
}

TEST(Recursive, ListLength_tailrecursive) {
  using build_object::tvar;
  std::string_view source = R"(


length_tl(acc_args) {
  acc = acc_args[4];
  args = acc_args[2];
  x = args[4];
  one = 1;

  one_v = int_to_v(one);

  test(x,one);
  ans_v = if (jne) then {
    this_f = args[2];
    cnt = x[2];
    tl = cnt[3];
    two = 2;
    acc_p1 = add(acc,two);
    f_tl = apply_fn(this_f,tl);
    f_tl_nacc = apply_fn(f_tl,acc_p1);
    ans = f_tl_nacc;
    return ans;
  } else {
    return acc;
  };
  return ans_v;
}

length(args) {
  list = args[4];
  zero_v = 1;
  fun = __fun_block_length_tl__;
  fl = apply_fn(fun, list);
  ans = apply_fn(fl,zero_v);
  return ans;
}

)";
  testcase_params tp =
      {.test_function="length", .call_style=progressive_application, .curriables={{"length", 1},
                                                                                  {"length_tl",
                                                                                   2}}};
  build_object::value list = tvar(3); //empty-list
  for (size_t l = 0; l < 10; ++l) {
    tp.expected_return = l;
    test_ir_build(source, {list}, tp);
    list = tvar(5, {l, std::move(list)});
  }
}

TEST(Recursive, ListLength_tailrecursive_annotated) {
  using build_object::tvar;
  std::string_view source = R"(


length_tl(acc_args : non_trivial) {
  acc : unboxed = acc_args[4];
  args : non_trivial = acc_args[2];
  x : non_global = args[4];
  one : trivial = 1;

  one_v : trivial = int_to_v(one);

  test(x,one);
  ans_v : unboxed = if (jne) then {
    this_f : global = args[2];
    cnt : non_trivial = x[2];
    tl : non_global = cnt[3];
    two : trivial = 2;
    acc_p1 : unboxed = add(acc,two);
    f_tl : non_trivial = apply_fn(this_f,tl);
    f_tl_nacc : unboxed = apply_fn(f_tl,acc_p1);
    ans : unboxed = f_tl_nacc;
    return ans;
  } else {
    return acc;
  };
  return ans_v;
}

length(args : non_trivial) {
  list = args[4];
  zero_v = 1;
  fun = __fun_block_length_tl__;
  fl = apply_fn(fun, list);
  ans = apply_fn(fl,zero_v);
  return ans;
}

)";
  testcase_params tp =
      {.test_function="length", .allocate_input_dynamically=true, .call_style=progressive_application, .curriables={
          {"length", 1},
          {"length_tl",
           2}}};
  build_object::value list = tvar(3); //empty-list
  for (size_t l = 0; l < 10; ++l) {
    tp.expected_return = l;
    test_ir_build(source, {list}, tp);
    list = tvar(5, {l, std::move(list)});
  }
}

//TODO: ApplyList, ListSum

TEST(IO, Stdout) {
  test_ir_build("", {42}, {
      .test_function = "println_int",
      .call_style=progressive_application,
      .curriables = {
          {"println_int", 1}
      },
      .expected_return = 0,
      .expected_stdout = "42\n",
      .expected_stderr = "",
  });
}

TEST(IO, Stderr) {
  test_ir_build("", {42}, {
      .test_function = "println_int_err",
      .call_style=progressive_application,
      .curriables = {
          {"println_int_err", 1}
      },
      .expected_return = 0,
      .expected_stdout = "",
      .expected_stderr = "42\n",
  });
}

TEST(Memory, DestroyABlock) {
  static constexpr std::string_view source = R"(
ignore (x) {
  zero = 0;
  unit = int_to_v(zero);
  return unit;
}
)";
  test_ir_build(source, {{1, 2, 3, 4}}, {
      .test_function = "ignore",
      .allocate_input_dynamically = true,
      .call_style=as_args,
      .curriables = {
          {"ignore", 1}
      },
      .expected_return = 0,
      .debug_log=true,
      .expected_stdout = MatchesRegex("decrement block 0x................ to 0\n"
                                      "destroying block of size 4 at 0x................\n"),
      .expected_stderr = "",
  });

}

TEST(Memory, DestroyCallDestructor) {
  static constexpr std::string_view source = R"(
print_box (args : non_trivial) {
  boxed_int : boxed = args[4];
  pint = __fun_block_println_int__;
  x : trivial = boxed_int[2];
  unit : trivial = apply_fn(pint,x);
  return unit;
}

make_epitaffable_box(args : non_trivial) {
  x : trivial = args[4];
  tuple : non_trivial = malloc(4);
  c3 = 3;
  c2 = 2;
  pb = __fun_block_print_box__;
  tuple[0] := c3;
  tuple[1] := c3;
  tuple[2] := x;
  tuple[3] := pb;
  return tuple;
}

test_function(x){
  meb = __fun_block_make_epitaffable_box__;
  box : non_trivial = apply_fn(meb,x);
  unit = 1;
  return unit;
}
)";
  test_ir_build(source, {1546}, {
      .call_style=as_args,
      .curriables = {
          {"println_int", 1}, {"make_epitaffable_box", 1}, {"print_box", 1}
      },
      .expected_return = 0,
      .debug_log = true,
      .expected_stdout = MatchesRegex("decrement block 0x................ to 0\n"
                                      "destroying block of size 3 at 0x................\n"
                                      "decrement block 0x................ to 0\n"
                                      "increment block 0x................ to 2\n"
                                      "decrement block 0x................ to 0\n"
                                      "destroying block of size 3 at 0x................\n"
                                      "decrement block 0x................ to 1\n"
                                      "decrement block 0x................ to 0\n"
                                      "destroying block of size 1 at 0x................\n"
                                      "1546\n"
                                      "decrement block 0x................ to 0\n"
                                      "destroying block of size 3 at 0x................\n"),
  });
}

//TODO: testing idea
// make a function that take a pair and an index and return the indexed object; both object have an attached destructor

}
