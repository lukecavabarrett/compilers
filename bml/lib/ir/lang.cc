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

std::optional<destroy_class_t> destroy_class_of_string(std::string_view str) {
  static std::unordered_map<std::string_view, destroy_class_t> const table =
      {{"unboxed", unboxed}, {"global", global}, {"non_trivial", non_trivial}, {"Value", value},
       {"trivial", trivial}, {"boxed", boxed}, {"non_global", non_global}};
  if (auto it = table.find(str);it != table.end()) {
    return it->second;
  } else {
    return {};
  }
}

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
  if (t == IDENTIFIER)return "identifier";
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
    throw std::runtime_error(
        formatter() << "cannot parse anymore: " << int(to_parse.front()) << to_parse >> formatter::to_str);
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
std::string_view tokenizer::get_source() const {
  return source;
}

}

std::unordered_map<uint64_t, std::string> var::maybe_names;
std::vector<destroy_class_t> var::destroy_classes;

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
  auto print_destroy = [&](size_t i,size_t offset,bool newline=false){
    assert(i<=destroys.size());
    if(destroys.at(i).empty())return;
    while(offset--)os << "  ";
    os << "//destroying ";
    bool comma = false;
    for(var v : destroys.at(i)){
      if(comma)os<<", ";
      comma=true;
      os<<v;
    }
    if(newline)os<<"\n";
  };
  print_destroy(0,offset,true);
  size_t idx = 0;
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
    os << "; ";
    ++idx;
    print_destroy(idx,1);
    os << "\n";
  }
  for (int i = offset; i--;)os << "  ";
  os << "return ";
  ret.print(os);
  os << ";\n";
}
void scope::parse(parse::tokenizer &tk, std::unordered_map<std::string_view, var> &names) {
  using namespace parse;
  this->destroys.clear();
  this->body.clear();
  std::vector<std::string_view> inserted_names;
  while (!tk.empty() && tk.peek() != RETURN) {
    bool had_star = false;
    if (tk.peek() == STAR) {
      tk.pop();
      had_star = true;
    }
    tk.expect_peek(IDENTIFIER);
    auto i = tk.pop().sv;
    std::optional<destroy_class_t> dc;
    if (tk.peek() == COLON) {
      tk.expect_pop(COLON);
      tk.expect_peek(IDENTIFIER);
      dc = destroy_class_of_string(tk.peek_sv());
      if (!dc) throw error::report_token(tk.peek_sv(), "Specifier", "is not a recognized destroyability class");

      tk.pop();
    }

    if (tk.peek() == EQUAL) {
      assert(!had_star);
      // vardef
      assert(!names.contains(i));
      var v(i);
      if(dc.has_value())v.destroy_class() = dc.value();
      names.try_emplace(i, v);
      inserted_names.push_back(i);
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
            assert(std::from_chars(tk.peek_sv().data(), tk.peek_sv().data() + tk.peek_sv().size(), addr).ec
                       == std::errc());
            tk.pop();
            tk.expect_pop(BRACKET_CLOSE);
            push_back(instruction::assign{.dst = v, .src=rhs_expr::memory_access{.base = names.at(a), .block_offset = addr}});

          } else if (a == "malloc") {
            //malloc
            tk.expect_pop(PARENS_OPEN);
            tk.expect_peek(CONSTANT);
            uint64_t val;
            assert(
                std::from_chars(tk.peek_sv().data(), tk.peek_sv().data() + tk.peek_sv().size(), val).ec == std::errc());
            tk.pop();
            tk.expect_pop(PARENS_CLOSE);
            push_back(instruction::assign{.dst = v, .src=rhs_expr::malloc{.size = val}});
          } else if (a == "apply_fn") {
            //apply_fn
            tk.expect_pop(PARENS_OPEN);
            tk.expect_peek(IDENTIFIER);
            assert(names.contains(tk.peek_sv()));
            var vf = names.at(tk.peek_sv());
            tk.expect_pop(IDENTIFIER);
            tk.expect_pop(COMMA);
            tk.expect_peek(IDENTIFIER);
            assert(names.contains(tk.peek_sv()));
            var vx = names.at(tk.peek_sv());
            tk.expect_pop(IDENTIFIER);
            tk.expect_pop(PARENS_CLOSE);
            push_back(instruction::assign{.dst = v, .src=rhs_expr::apply_fn{.f = vf ,.x = vx}});
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
              push_back(instruction::assign{.dst = v, .src=rhs_expr::binary_op{.op=op, .x1 = op1, .x2 = op2}});
            } else {
              //unary operand
              auto op = rhs_expr::unary_op::string_to_op(a);
              push_back(instruction::assign{.dst = v, .src=rhs_expr::unary_op{.op=op, .x = op1}});
            }
            tk.expect_pop(PARENS_CLOSE);
          } else if (names.contains(a)) {
            //copy
            push_back(instruction::assign{.dst = v, .src=rhs_expr::copy{.v = names.at(a)}});
          } else {
            //global
            push_back(instruction::assign{.dst = v, .src=rhs_expr::global{.name = std::string(a)}});

          }
          break;
        }
        case CONSTANT: {
          uint64_t val;
          assert(
              std::from_chars(tk.peek_sv().data(), tk.peek_sv().data() + tk.peek_sv().size(), val).ec == std::errc());
          tk.pop();
          push_back(instruction::assign{.dst = v, .src=rhs_expr::constant(val)});
          break;
        };

        case IF: {
          // ternary
          tk.expect_pop(IF);
          tk.expect_pop(PARENS_OPEN);
          tk.expect_peek(IDENTIFIER);
          auto jinstr = tk.pop().sv;
          rhs_expr::branch b = std::make_unique<ternary>();
          b->cond = ternary::parse_jinstr(jinstr);
          tk.expect_pop(PARENS_CLOSE);
          tk.expect_pop(THEN);
          tk.expect_pop(CURLY_OPEN);
          b->nojmp_branch.parse(tk, names);
          tk.expect_pop(CURLY_CLOSE);
          tk.expect_pop(ELSE);
          tk.expect_pop(CURLY_OPEN);
          b->jmp_branch.scope::parse(tk, names);
          tk.expect_pop(CURLY_CLOSE);
          push_back(instruction::assign{.dst=v, .src=rhs_expr::branch(std::move(b))});
          break;
        };
        case STAR: {
          //memory access
          tk.pop();
          tk.expect_peek(IDENTIFIER);
          auto src = tk.pop().sv;
          assert(names.contains(src));
          push_back(instruction::assign{.dst = v, .src=rhs_expr::memory_access{.base=names.at(src), .block_offset = 0}});
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
      push_back(instruction::cmp_vars{.v1 = op1, .v2 = op2, .op = op.value()});

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
      push_back(instruction::write_uninitialized_mem{.base = names.at(i), .block_offset = addr, .src = names.at(src)});
    }
    tk.expect_pop(SEMICOLON);
  }
  tk.expect_pop(RETURN);
  tk.expect_peek(IDENTIFIER);
  auto retv = tk.pop().sv;
  tk.expect_pop(SEMICOLON);
  assert(names.contains(retv));
  ret = names.at(retv);
  for (std::string_view s : inserted_names)names.erase(s);
}

void function::parse(parse::tokenizer &tk) {
  try {
    using namespace parse;
    tk.expect_peek(IDENTIFIER);
    name = tk.pop().sv;
    std::unordered_map<std::string_view, var> names;
    tk.expect_pop(PARENS_OPEN);
    while (tk.peek() != PARENS_CLOSE) {
      tk.expect_peek(IDENTIFIER);
      std::string_view param_name = tk.pop().sv;
      std::optional<destroy_class_t> dc;
      if (tk.peek() == COLON) {
        tk.expect_pop(COLON);
        tk.expect_peek(IDENTIFIER);
        auto dc = destroy_class_of_string(tk.peek_sv());
        if (!dc) throw error::report_token(tk.peek_sv(), "Specifier", "is not a recognized destroyability class");
        tk.expect_pop(IDENTIFIER);
      }
      if (tk.peek() == COMMA)tk.pop();
      assert(!names.contains(param_name));
      var v(param_name);
      if(dc.has_value())v.destroy_class() = dc.value();
      names.try_emplace(param_name, v);
      args.emplace_back(param_name, v);
    }
    tk.expect_pop(PARENS_CLOSE);
    tk.expect_pop(CURLY_OPEN);
    scope::parse(tk, names);
    tk.expect_pop(CURLY_CLOSE);
  } catch (const util::error::message &e) {
    e.print(std::cout, tk.get_source(), "source.ir");
    throw std::runtime_error("ir parsing error");
  }

}
void function::print(std::ostream &os, size_t offset) const {
  os << name << "(";
  bool comma = false;
  for (const auto&[s, _] : args) {
    if (comma)os << ", ";
    comma = true;
    os << s;
  }
  os << ") {\n";
  scope::print(os, offset + 1);
  os << "}\n";
}

std::string scope::to_string() {
  std::stringstream s;
  print(s);
  return std::move(s.str());
}

}
