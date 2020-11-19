#include <lang.h>
#include <cassert>
#include <unordered_set>
#include <map>
#include <list>
#include <charconv>





namespace ir::lang {
std::ostream &operator<<(std::ostream &os, const var &v) {
  v.print(os);
  return os;
}

namespace parse {

namespace {
using namespace util;
bool allowed_in_identifier(char c) {
  return ::isalnum(c) || c == '_' || c == '\'';
}
template<typename T>
std::unique_ptr<T> with_loc(std::unique_ptr<T> &&p, std::string_view s) {
  p->loc = s;
  return std::move(p);
}

bool startswith_legal(std::string_view a, std::string_view b) {
  if (b.size() > a.size())return false;
  if (!std::equal(b.begin(), b.end(), a.begin()))return false;
  if (allowed_in_identifier(b.back()) && a.size() > b.size() && allowed_in_identifier(a[b.size()]))return false;
  return true;
}

namespace {
using namespace parse;
std::string_view token_type_to_string(token_type t) {
  for (const auto&[p, t_i] : tokens_map)if (t_i == t)return p;
  THROW_UNIMPLEMENTED
}

}

}

tokenizer::tokenizer(std::string_view source) : to_parse(source), source(source) { write_head(); }
token tokenizer::pop() {
  token t = head;
  write_head();
  return t;
}
token_type tokenizer::peek() const { return head.type; }
token tokenizer::peek_full() const { return head; }
std::string_view tokenizer::peek_sv() const { return head.sv; }
bool tokenizer::empty() const { return head.type == END_OF_INPUT; }
void tokenizer::write_head() {
  while (!to_parse.empty() && to_parse.front() <= 32)to_parse.remove_prefix(1);
  if (to_parse.empty()) {
    head = token{.sv=std::string_view(), .type=END_OF_INPUT};
    return;
  }
  for (const auto&[p, t] : tokens_map)
    if (startswith_legal(to_parse, p)) {
      head = token{.sv=std::string_view(to_parse.begin(), p.size()), .type=t};
      to_parse.remove_prefix(p.size());
      return;
    }

  if (isdigit(to_parse.front())) {
    //int literal
    auto end = std::find_if_not(to_parse.begin(), to_parse.end(), ::isdigit);
    head = {.sv=itr_sv(to_parse.begin(), end), .type=token_type::CONSTANT};
    to_parse.remove_prefix(end - to_parse.begin());
    return;
  }

  auto end = std::find_if_not(to_parse.begin(), to_parse.end(), allowed_in_identifier);
  if (to_parse.begin() == end) {
    throw std::runtime_error(formatter() << "cannot parse anymore: " << int(to_parse.front()) << to_parse >> formatter::to_str);
  }
  head = {.sv=itr_sv(to_parse.begin(), end), .type= token_type::IDENTIFIER};
  to_parse.remove_prefix(end - to_parse.begin());

}
void tokenizer::expect_pop(token_type t) {
  //TODO: make better error
  expect_peek(t);
  write_head();
}
void tokenizer::expect_peek(token_type t) {
  //TODO: make better error
  if (head.type != t) {
    throw error::expected_token_found_another(token_type_to_string(t), head.sv);
  }
}
void tokenizer::expect_peek_any_of(std::initializer_list<token_type> il) {
  if (std::find(il.begin(), il.end(), head.type) == il.end()) {
    throw error::unexpected_token(head.sv);
  }
}
void tokenizer::unexpected_token() {
  throw error::unexpected_token(head.sv);
  //throw std::runtime_error("expected \"TK\", found another");
}

}

std::unordered_map<uint64_t, std::string> var::maybe_names = {{0, "__argv"}};

rhs_expr::memory_access var::operator*() const {
  return rhs_expr::memory_access{.base = *this, .block_offset = 0};
}
rhs_expr::memory_access var::operator[](size_t idx) const {
  return rhs_expr::memory_access{.base = *this, .block_offset = idx};
}
var::operator rhs_expr::copy() const {
  return rhs_expr::copy{.v=*this};
}
instruction::assign var::assign(rhs_expr::t &&src) const {
  return instruction::assign{.dst = *this, .src = std::move(src)};
}

scope &scope::operator<<(instruction::t &&i) {
  push_back(std::move(i));
  return *this;
}

void scope::push_back(instruction::t &&i) {
  body.push_back(std::move(i));
}
void scope::print(std::ostream &os, size_t offset) const {
  using namespace util;
  for (auto &it : body) {
    for (int i = offset; i--;)os << "  ";
    std::visit(overloaded{
        [&](const instruction::assign &a) {
          os << a.dst << " = ";
          std::visit(overloaded{
              [&](const rhs_expr::constant &ce) { os << ce.v; },
              [&](const rhs_expr::global &g) { os << g.name; },
              [&](const rhs_expr::copy &ce) { ce.v.print(os); },
              [&](const rhs_expr::memory_access &ma) {
                if (ma.block_offset == 0) os << "*";
                ma.base.print(os);
                if (ma.block_offset) os << "[" << ma.block_offset << "]";
              },
              [&](const rhs_expr::malloc &m) { os << "malloc(" << m.size << ")"; },
              [&](const rhs_expr::apply_fn &f) { os << "apply_fn(" << f.f << ", " << f.x << ")"; },
              [&](const rhs_expr::branch &b) {
                os << "if (" <<
                   b->ops_to_string() << "){\n";
                b->nojmp_branch.print(os, offset + 1);
                for (int i = offset; i--;)os << "  ";
                os << "} else {\n";
                b->jmp_branch.print(os, offset + 1);
                for (int i = offset; i--;)os << "  ";
                os << "}";
              },
              [&](const rhs_expr::unary_op &u) { os << rhs_expr::unary_op::ops_to_string(u.op) << "(" << u.x << ")"; },
              [&](const rhs_expr::binary_op &b) {
                os << rhs_expr::binary_op::ops_to_string(b.op) << "(" << b.x1 << "," << b.x2 << ")";
              }
          }, a.src);
        },
        [&](const instruction::write_uninitialized_mem &a) {
          os << a.base << "[" << a.block_offset << "]" << " := " << a.src;
        },
        [&](const instruction::cmp_vars &a) {
          os << instruction::cmp_vars::ops_to_string(a.op) << "(" << a.v1 << ", " << a.v2 << ")";
        },
    }, it);
    os << ";\n";
  }
  for (int i = offset; i--;)os << "  ";
  os << "return ";
  ret.print(os);
  os << ";\n";
}
scope scope::parse(parse::tokenizer &tk, std::unordered_map<std::string_view, var> &names) {
  scope s;
  using namespace parse;
  while (!tk.empty() && tk.peek() != RETURN) {
    bool had_star = false;
    if (tk.peek() == STAR) {
      tk.pop();
      had_star = true;
    }
    tk.expect_peek(IDENTIFIER);
    auto i = tk.pop().sv;
    if (tk.peek() == EQUAL) {
      assert(!had_star);
      //TODO: vardef
      assert(!names.contains(i));
      var v(i);
      names.try_emplace(i, v);
      tk.expect_pop(EQUAL);
      switch (tk.peek()) {

        case IDENTIFIER: {
          //copy or global or memory access or operation
          auto a = tk.pop().sv;
          if (tk.peek() == BRACKET_OPEN) {
            //memory access
            assert(names.contains(a));
            tk.expect_pop(BRACKET_OPEN);
            tk.expect_peek(CONSTANT);
            size_t addr = 0;
            assert(std::from_chars(tk.peek_sv().data(), tk.peek_sv().data() + tk.peek_sv().size(), addr).ec == std::errc());
            tk.pop();
            tk.expect_pop(BRACKET_CLOSE);
            s.push_back(instruction::assign{.dst = v, .src=rhs_expr::memory_access{.base = names.at(a), .block_offset = addr}});

          } else if (a == "malloc") {
            //malloc
            tk.expect_pop(PARENS_OPEN);
            tk.expect_peek(CONSTANT);
            uint64_t val;
            assert(std::from_chars(tk.peek_sv().data(), tk.peek_sv().data() + tk.peek_sv().size(), val).ec == std::errc());
            tk.pop();
            tk.expect_pop(PARENS_CLOSE);
            s.push_back(instruction::assign{.dst = v, .src=rhs_expr::malloc{.size = val}});
          } else if (a == "apply_fn") {
            //malloc
            THROW_UNIMPLEMENTED
          } else if (tk.peek() == PARENS_OPEN) {
            //operation
            tk.expect_pop(PARENS_OPEN);
            tk.expect_peek(IDENTIFIER);
            assert(names.contains(tk.peek_sv()));
            var op1 = names.at(tk.pop().sv);
            tk.expect_peek_any_of({PARENS_CLOSE, COMMA});
            if (tk.peek() == COMMA) {
              //binary operand
              tk.expect_pop(COMMA);
              tk.expect_peek(IDENTIFIER);
              assert(names.contains(tk.peek_sv()));
              var op2 = names.at(tk.pop().sv);
              auto op = rhs_expr::binary_op::string_to_op(a);
              s.push_back(instruction::assign{.dst = v, .src=rhs_expr::binary_op{.op=op, .x1 = op1, .x2 = op2}});
            } else {
              //unary operand
              auto op = rhs_expr::unary_op::string_to_op(a);
              s.push_back(instruction::assign{.dst = v, .src=rhs_expr::unary_op{.op=op, .x = op1}});
            }
            tk.expect_pop(PARENS_CLOSE);
          } else if (names.contains(a)) {
            //copy
            s.push_back(instruction::assign{.dst = v, .src=rhs_expr::copy{.v = names.at(a)}});
          } else {
            //global
            s.push_back(instruction::assign{.dst = v, .src=rhs_expr::global{.name = std::string(a)}});

          }
          break;
        }
        case CONSTANT: {
          uint64_t val;
          assert(std::from_chars(tk.peek_sv().data(), tk.peek_sv().data() + tk.peek_sv().size(), val).ec == std::errc());
          tk.pop();
          s.push_back(instruction::assign{.dst = v, .src=rhs_expr::constant(val)});
          break;
        };

        case IF: {
          //TODO: ternary
          tk.expect_pop(IF);
          tk.expect_pop(PARENS_OPEN);
          tk.expect_peek(IDENTIFIER);
          auto jinstr = tk.pop().sv;
          rhs_expr::branch b = std::make_unique<ternary>();
          b->cond = ternary::parse_jinstr(jinstr);
          tk.expect_pop(PARENS_CLOSE);
          tk.expect_pop(THEN);
          tk.expect_pop(CURLY_OPEN);
          b->nojmp_branch = scope::parse(tk, names);
          tk.expect_pop(CURLY_CLOSE);
          tk.expect_pop(ELSE);
          tk.expect_pop(CURLY_OPEN);
          b->jmp_branch = scope::parse(tk, names);
          tk.expect_pop(CURLY_CLOSE);
          s.push_back(instruction::assign{.dst=v, .src=rhs_expr::branch(std::move(b))});
          break;
        };
        case STAR: {
          //memory access
          tk.pop();
          tk.expect_peek(IDENTIFIER);
          auto src = tk.pop().sv;
          assert(names.contains(src));
          s.push_back(instruction::assign{.dst = v, .src=rhs_expr::memory_access{.base=names.at(src), .block_offset = 0}});
          break;
        }
        default:THROW_INTERNAL_ERROR;
      }

    } else if (auto op = instruction::cmp_vars::parse_op(i); op.has_value()) {
      assert(!had_star);
      // cmp_vars
      tk.expect_pop(PARENS_OPEN);
      tk.expect_peek(IDENTIFIER);
      assert(names.contains(tk.peek_sv()));
      var op1 = names.at(tk.pop().sv);
      tk.expect_pop(COMMA);
      tk.expect_peek(IDENTIFIER);
      assert(names.contains(tk.peek_sv()));
      var op2 = names.at(tk.pop().sv);
      tk.expect_pop(PARENS_CLOSE);
      s.push_back(instruction::cmp_vars{.v1 = op1, .v2 = op2, .op = op.value()});

    } else {
      //memory assignment
      assert(names.contains(i));
      size_t addr = 0;
      if (!had_star) {
        tk.expect_pop(BRACKET_OPEN);
        tk.expect_peek(CONSTANT);
        assert(std::from_chars(tk.peek_sv().data(), tk.peek_sv().data() + tk.peek_sv().size(), addr).ec == std::errc());
        tk.pop();
        tk.expect_pop(BRACKET_CLOSE);
      }
      tk.expect_pop(ASSIGN);
      tk.expect_peek(IDENTIFIER);
      auto src = tk.pop().sv;
      assert(names.contains(src));
      s.push_back(instruction::write_uninitialized_mem{.base = names.at(i), .block_offset = addr, .src = names.at(src)});
    }
    tk.expect_pop(SEMICOLON);
  }
  tk.expect_pop(RETURN);
  tk.expect_peek(IDENTIFIER);
  auto retv = tk.pop().sv;
  tk.expect_pop(SEMICOLON);
  assert(names.contains(retv));
  s.ret = names.at(retv);
  return s;

}
scope scope::parse(std::string_view source) {
  parse::tokenizer tk(source);
  std::unordered_map<std::string_view, var> names = {{"argv", argv_var}};
  try {

    return parse(tk, names);
  } catch (const util::error::message &e) {
    e.print(std::cout, source, "source.ir");
    throw std::runtime_error("ir parsing error");
  }
}
std::string scope::to_string() {
  std::stringstream s;
  print(s);
  return std::move(s.str());
}

}
