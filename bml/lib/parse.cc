#include <parse.h>
#include <unordered_map>
#include <algorithm>
#include <cassert>
#include <util.h>

namespace {

bool allowed_in_identifier(char c) {
  return ::isalnum(c) || c == '_';
}

bool startswith_legal(std::string_view a, std::string_view b) {
  if (b.size() > a.size())return false;
  if (!std::equal(b.begin(), b.end(), a.begin()))return false;
  if (allowed_in_identifier(b.back()) && a.size() > b.size() && allowed_in_identifier(a[b.size()]))return false;
  return true;
}

namespace {
using namespace parse;
std::string_view token_type_to_string(token_type t){
  for (const auto&[p, t_i] : tokens_map)if(t_i==t)return p;
  throw std::runtime_error("parsing of string literal unimplemented");
}

}

}

namespace parse {
tokenizer::tokenizer(std::string_view source) : to_parse(source) { write_head(); }
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

  if (to_parse.front() == '\"') {
    //string literal
    throw std::runtime_error("parsing of string literal unimplemented");
  }

  if (to_parse.front() == '.') {
    //float literal
    auto end = std::find_if_not(to_parse.begin() + 1, to_parse.end(), ::isdigit);
    head = {.sv=itr_sv(to_parse.begin(), end), .type=token_type::LITERAL};
    to_parse.remove_prefix(end - to_parse.begin());
    return;
  }

  if (isdigit(to_parse.front())) {
    //int or float literal
    auto end = std::find_if_not(to_parse.begin(), to_parse.end(), ::isdigit);
    if (end != to_parse.end() && *end == '.')end = std::find_if_not(end + 1, to_parse.end(), ::isdigit);
    head = {.sv=itr_sv(to_parse.begin(), end), .type=token_type::LITERAL};
    to_parse.remove_prefix(end - to_parse.begin());
    return;
  }

  auto end = std::find_if_not(to_parse.begin(), to_parse.end(), allowed_in_identifier);
  if (to_parse.begin() == end) {
    throw std::runtime_error(formatter() << "cannot parse anymore: " << int(to_parse.front()) << to_parse >> formatter::to_str);
  }
  head = {.sv=itr_sv(to_parse.begin(), end), .type= ::isupper(to_parse.front()) ? token_type::CAP_NAME : token_type::IDENTIFIER};
  to_parse.remove_prefix(end - to_parse.begin());

}
void tokenizer::expect_pop(token_type t) {
  //TODO: make better error
  expect_peek(t);
  write_head();
}
void tokenizer::expect_peek(token_type t) {
  //TODO: make better error
  if (head.type != t){
    throw std::runtime_error("expected \"TK\", found another");
  }
}
void tokenizer::unexpected_token(){
  throw std::runtime_error("expected \"TK\", found another");
}

}
namespace ast {

namespace expression {
/*

 expressions

 E -> Es | IF E THEN E ELSE E |  MATCH E WITH ( | E  -> E )*+ | LET ... |
 Es -> Et | Et SEMICOLON Es                 i.e. a list of E2s, any associativity
 Et -> Ef COMMA Et | Ef
 Ef -> Ef Ep | Ep                   i.e. a list of E4s, left associativity
 Ep -> () | ( E ) | Literal | Ex
 Ex -> Modulename DOT Ex | Identififer | Constructor
*/
namespace {
ptr parse_e_s(tokenizer &tk);
ptr parse_e_t(tokenizer &tk);
ptr parse_e_f(tokenizer &tk);
ptr parse_e_p(tokenizer &tk);
bool parse_e_p_first(token_type);

ptr parse_e_s(tokenizer &tk) {
  auto e2 = parse_e_t(tk);
  while (!tk.empty() && tk.peek() == SEMICOLON) {
    tk.pop();
    auto ne2 = parse_e_t(tk);
    e2 = std::make_unique<seq>(std::move(e2), std::move(ne2), itr_sv(e2->loc.begin(), ne2->loc.end()));
  }
  return std::move(e2);
}

ptr parse_e_t(tokenizer &tk) {
  auto e3 = parse_e_f(tk);
  if (tk.peek() != COMMA)return std::move(e3);
  auto e2 = std::make_unique<build_tuple>(std::string_view());
  e2->args.emplace_back(std::move(e3));
  while (tk.peek() == COMMA) {
    tk.pop();
    auto e3 = parse_e_f(tk);
    e2->args.emplace_back(std::move(e3));
  }
  e2->loc = itr_sv(e2->args.front()->loc.begin(), e2->args.back()->loc.end());
  return std::move(e2);
}

ptr parse_e_f(tokenizer &tk) {
  auto e4 = parse_e_p(tk);
  while (parse_e_p_first(tk.peek())) {
    auto ne4 = parse_e_p(tk);
    e4 = std::make_unique<fun_app>(std::move(e4), std::move(ne4), itr_sv(e4->loc.begin(), ne4->loc.end()));
  }
  return std::move(e4);
}

bool parse_e_p_first(token_type t) {
  return is_in(t, {TRUE, FALSE, LITERAL, IDENTIFIER, CAP_NAME, PARENS_OPEN});
}
ptr parse_e_p(tokenizer &tk) {
  switch (tk.peek()) {
    case TRUE:
    case FALSE:
    case LITERAL: {
      return std::make_unique<literal>(ast::literal::parse(tk.pop()), tk.peek_sv());
    }
    case IDENTIFIER:
    case CAP_NAME: {
      return std::make_unique<identifier>(tk.pop().sv);
    }
      //TODO-someday : implement dot notation
    case PARENS_OPEN: {
      auto loc_start = tk.pop().sv.begin();
      if (tk.peek() != PARENS_CLOSE) {
        auto e = expression::parse(tk);
        if (tk.peek() != PARENS_CLOSE)throw std::runtime_error("expected \")\"");
        //IDEA: enlarge
        e->loc = itr_sv(loc_start, tk.pop().sv.end());
        return std::move(e);
      } else {
        return std::make_unique<literal>(std::make_unique<ast::literal::unit>(), itr_sv(loc_start, tk.pop().sv.end())) ;
      }
    }
    default:tk.unexpected_token();
  }
}

}

ptr parse(tokenizer &tk) {
    switch (tk.peek()) {
      case IF: {
        auto loc_start = tk.pop().sv.begin();
        auto condition = parse(tk);
        tk.expect_pop(THEN);
        auto true_branch = parse(tk);
        tk.expect_pop(ELSE);
        auto false_branch = parse(tk);
        return std::make_unique<if_then_else>(std::move(condition), std::move(true_branch), std::move(false_branch), itr_sv(loc_start, false_branch->loc.end()));
      }
      case MATCH: {
        auto loc_start = tk.pop().sv.begin();
        auto what = parse(tk);
        match_with::ptr mtc = std::make_unique<match_with>(std::move(what), std::string_view());
        tk.expect_pop(WITH);
        tk.expect_peek(PIPE);
        while (tk.peek() == PIPE) {
          tk.pop();
          auto m = ast::matcher::parse(tk);
          tk.expect_pop(ARROW);
          auto e = parse(tk);
          mtc->branches.push_back({.pattern=std::move(m), .result=std::move(e)});
        }
        mtc->loc = itr_sv(loc_start, mtc->branches.back().result->loc.end());
        return std::move(mtc);
      }
      case LET: {
        auto loc_start = tk.peek_sv().begin();
        auto d = definition::parse(tk);
        tk.expect_pop(IN);
        auto e = parse(tk);
        return std::make_unique<let_in>(std::move(d), std::move(e), itr_sv(loc_start, e->loc.end()));
      }
      default:return parse_e_s(tk);
    }
}

}

namespace matcher {
/*
 matchers

 M -> M1
 M1 -> M2 | M2 COMMA M1
 M2 -> M3 | Constructor | Constructor M3
 M3 -> () | _ | binder | Literal | (M)


 M -> _ | Identifier | Literal | Constructor M | Tuple of M


 */
namespace {

ptr parse_m_1(tokenizer &tk);
ptr parse_m_2(tokenizer &tk);
ptr parse_m_3(tokenizer &tk);
bool parse_m_3_first(const token &t);

ptr parse_m_1(tokenizer &tk) {
  auto m2 = parse_m_2(tk);
  if (tk.empty() || tk.peek() != COMMA)return std::move(m2);
  auto m1 = std::make_unique<tuple_matcher>(std::string_view());
  m1->args.emplace_back(std::move(m2));
  while (tk.peek() == COMMA) {
    tk.expect_pop(COMMA);
    auto m2 = parse_m_2(tk);
    m1->args.emplace_back(std::move(m2));
  }
  m1->loc = itr_sv(m1->args.front()->loc.begin(), m1->args.back()->loc.end());
  return std::move(m1);
}

ptr parse_m_2(tokenizer &tk) {
  switch (tk.peek()) {
    case CAP_NAME: {
      //TODO: Maybe better structure for loc?
      auto begin_sv = tk.pop().sv;
      if (!parse_m_3_first(tk.peek_full())) return std::make_unique<constructor_matcher>(begin_sv);

      auto m = parse_m_3(tk);
      return std::make_unique<constructor_matcher>(std::move(m), itr_sv(begin_sv.begin(), m->loc.end()), begin_sv);
    }
    default: { return parse_m_3(tk); }
  }
}

bool parse_m_3_first(const token &t) {
  return is_in(t.type, {TRUE, FALSE, LITERAL, IDENTIFIER, PARENS_OPEN, UNDERSCORE});
}
bool parse_m_1_first(const token &t) {
  return is_in(t.type, {TRUE, FALSE, LITERAL, IDENTIFIER, PARENS_OPEN, CAP_NAME, UNDERSCORE});
}

ptr parse_m_3(tokenizer &tk) {
  switch (tk.peek()) {
    case PARENS_OPEN: {
      auto loc_start = tk.pop().sv.begin();
      if (tk.peek() == PARENS_CLOSE)return std::make_unique<literal_matcher>(std::make_unique<ast::literal::unit>(), itr_sv(loc_start, tk.pop().sv.end()));
      auto m = parse_m_1(tk);
      tk.expect_peek(PARENS_CLOSE);
      m->loc = itr_sv(loc_start, tk.pop().sv.end());
      return std::move(m);
    }
    case UNDERSCORE: {
      return std::make_unique<anonymous_universal_matcher>(tk.pop().sv);
    }
    case IDENTIFIER: {
      return std::make_unique<universal_matcher>(tk.pop().sv);
    }
    case TRUE:
    case FALSE:
    case LITERAL: {
      return std::make_unique<literal_matcher>(ast::literal::parse(tk.pop()), tk.peek_sv());
    }
    default: {
      throw std::runtime_error("unexpected_token");
    }
  }

}
}
ptr parse(tokenizer &tk) {
  return parse_m_1(tk);
}
}

namespace literal {

ptr parse(const token &t);

}

namespace definition {
ptr parse(tokenizer &tk) {
  auto loc_start = tk.peek_sv().begin();
  tk.expect_pop(LET);
  bool rec = false;
  if (tk.peek() == REC) {
    rec = true;
    tk.expect_pop(REC);
  }
  definition::ptr defs = std::make_unique<t>();
  do {
    if(tk.peek()==AND)tk.expect_pop(AND);
    auto loc_start = tk.peek_sv().begin();
    auto m = matcher::parse(tk);
    if (dynamic_cast<matcher::universal_matcher *>(m.get()) && tk.peek() != EQUAL) {
      //function definition
      function::ptr fundef = std::make_unique<function>();
      fundef->name.reset(dynamic_cast<matcher::universal_matcher *>(m.release()));
      while (tk.peek() != EQUAL) {
        auto m = matcher::parse(tk);
        fundef->args.push_back(std::move(m));
      }
      tk.expect_pop(EQUAL);
      auto e = expression::parse(tk);
      fundef->body = std::move(e);
      fundef->loc = itr_sv(fundef->name->loc.begin(), fundef->body->loc.end());
      defs->defs.push_back(std::move(fundef));
    } else {
      //value binding
      tk.expect_pop(EQUAL);
      auto e = expression::parse(tk);
      defs->defs.push_back(std::move(std::make_unique<value>(std::move(m), std::move(e), itr_sv(loc_start, e->loc.end()))));
    }
  } while (tk.peek() == AND);
  defs->loc = itr_sv(loc_start, defs->defs.back()->loc.end());
  return std::move(defs);

}

}

}

ast::literal::ptr ast::literal::parse(const token &t) {
  switch (t.type) {
    case TRUE:return std::make_unique<boolean>(true);
    case FALSE:return std::make_unique<boolean>(false);
    case LITERAL: {
      if (int64_t n;std::from_chars(t.sv.begin(), t.sv.end(), n).ec == std::errc())return std::make_unique<integer>(n);
      //TODO: floating point literal, string literal
      throw std::runtime_error("unimplemented floating point literal, string literal");
    }
    default: throw std::runtime_error("trying to parse a literal");
  }

}
