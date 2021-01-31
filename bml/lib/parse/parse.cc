#include <parse.h>
#include <unordered_map>
#include <algorithm>
#include <cassert>
#include <util/util.h>

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
  throw std::runtime_error(AT "parsing of string literal unimplemented");
}

}

}

namespace patterns {

template<token_type... sep>
struct tk_sep {
  typedef token sep_t;
  static bool has_sep(const tokenizer &tk) { return ((tk.peek() == sep) || ... ); }
  static sep_t consume_sep(tokenizer &tk) {
    tk.expect_peek_any_of({sep...});
    return tk.pop();
  }
};

template<auto F>
struct peek_sep {
  typedef bool sep_t;
  static bool has_sep(const tokenizer &tk) { return F(tk.peek()); }
  static sep_t consume_sep(tokenizer &tk) { return true; }
};

template<typename vec_type, auto vec_ptr, typename sep, auto sub_method>
std::invoke_result_t<decltype(sub_method), tokenizer &> parse_vec(tokenizer &tk) {
  auto p = sub_method(tk);
  if (!sep::has_sep(tk))return p;
  auto pv = std::make_unique<vec_type>();
  ((*pv.get()).*vec_ptr).push_back(std::move(p));
  while (sep::has_sep(tk)) {
    sep::consume_sep(tk);
    ((*pv.get()).*vec_ptr).push_back(sub_method(tk));
  }
  pv->loc = itr_sv(((*pv.get()).*vec_ptr).front()->loc.begin(), ((*pv.get()).*vec_ptr).back()->loc.end());
  return pv;
}

template<auto make_lr_type, typename sep, auto sub_method>
std::invoke_result_t<decltype(sub_method), tokenizer &> parse_fold_r(tokenizer &tk) {
  /*
   Parse something of the form
   A -> A
   A op1 B op2 C ->  Lr( Lr(A,B) , C )
   */
  auto l = sub_method(tk);
  while (sep::has_sep(tk)) {
    auto sepr = sep::consume_sep(tk);
    l = make_lr_type(std::move(l), sub_method(tk), std::move(sepr));
  }
  return l;
}
template<auto make_r_type, typename sep, auto sub_method>
std::invoke_result_t<decltype(sub_method), tokenizer &> parse_fold_unary_l(tokenizer &tk) {
  /*
   Parse something of the form
   A -> A
   op1 op2 A ->  L( L(A , op2) ,op1 )
   */
  if (!sep::has_sep(tk))return sub_method(tk);
  auto sepr = sep::consume_sep(tk);
  return make_r_type(parse_fold_unary_l<make_r_type, sep, sub_method>(tk), std::move(sepr));
}

template<auto make_lr_type, typename sep, auto sub_method>
std::invoke_result_t<decltype(sub_method), tokenizer &> parse_fold_l(tokenizer &tk) {
  /*
   Parse something of the form
   A -> A
   A op2 B op1 C ->  Lr( A , Lr(B,C) )
  */
  auto l = sub_method(tk);
  if (!sep::has_sep(tk))return l;
  auto sepr = sep::consume_sep(tk);
  return make_lr_type(std::move(l), parse_fold_l<make_lr_type, sep, sub_method>(tk), std::move(sepr));
}

}

namespace parse {
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
  if (to_parse.size() >= 2 && to_parse[0] == '(' && to_parse[1] == '*') {
    std::string_view start_comment(to_parse.begin(), 2);
    to_parse.remove_prefix(2);
    size_t nested_comment = 1;
    while (!to_parse.empty() && nested_comment) {
      if (to_parse.size() >= 2 && to_parse[0] == '(' && to_parse[1] == '*') {
        ++nested_comment;
        to_parse.remove_prefix(2);
      } else if (to_parse.size() >= 2 && to_parse[0] == '*' && to_parse[1] == ')') {
        --nested_comment;
        to_parse.remove_prefix(2);
      } else to_parse.remove_prefix(1);
    }
    if (nested_comment)throw parse::error::report_token(start_comment, "Begin of comment ", " is unmatched.");
    return write_head();
  }
  for (const auto&[p, t] : tokens_map)
    if (startswith_legal(to_parse, p)) {
      head = token{.sv=std::string_view(to_parse.begin(), p.size()), .type=t};
      to_parse.remove_prefix(p.size());
      return;
    }

  if (to_parse.front() == '\"') {
    //string literal
    size_t i = 1;
    bool previous_slash = false;
    while (i < to_parse.size() && to_parse[i] != 10 && (to_parse[i] != '\"' || previous_slash)) {
      if (!previous_slash && to_parse[i] == '\\')previous_slash = true;
      else previous_slash = false;
      ++i;
    }
    if (i == to_parse.size())throw parse::error::report_token(to_parse, "string literal", " is not terminated.");
    if (to_parse[i] == 10)
      throw parse::error::report_token(std::string_view(to_parse.begin(), i),
                                       "string literal",
                                       " is not terminated.");
    i += 1;
    head = token{.sv=std::string_view(to_parse.begin(), i), .type=LITERAL};
    to_parse.remove_prefix(i);
    return;
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
    throw std::runtime_error(
        formatter() << "cannot parse anymore: " << int(to_parse.front()) << to_parse >> formatter::to_str);
  }
  head = {.sv=itr_sv(to_parse.begin(), end), .type= ::isupper(to_parse.front()) ? token_type::CAP_NAME
                                                                                : token_type::IDENTIFIER};
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
ptr parse_e_a(tokenizer &tk);
// ptr parse_e_f(tokenizer &tk);
ptr parse_e_p(tokenizer &tk);
bool parse_e_p_first(token_type);

ptr make_seq(ptr &&a, ptr &&b, token) { return std::make_unique<seq>(std::move(a), std::move(b)); }
ptr make_destroy(ptr &&a, ptr &&b, token) {
  return std::make_unique<destroy>(std::move(a), std::move(b));
}
ptr parse_e_s(tokenizer &tk) {
  using namespace patterns;
  return parse_fold_r<make_seq, tk_sep<SEMICOLON>, parse_fold_r<make_destroy, tk_sep<DESTROY_ARROW>, parse_e_t>>(tk);
}

ptr parse_e_t(tokenizer &tk) {
  using namespace patterns;
  return parse_vec<tuple, &tuple::args, tk_sep<COMMA>, parse_e_a>(tk);
}

std::string_view make_unary_op(token t) {
  switch (t.type) {
    case PLUS:return "__unary_op__PLUS__";
    case MINUS:return "__unary_op__MINUS__";
    default: throw parse::error::report_token(t.sv, "", " is not a recognized prefix unary operator");
  }
}
bool is_binary_op(token_type t) {
  switch (t) {
    case STAR:
    case SLASH:
    case PLUS:
    case MINUS:
    case EQUAL:
    case LESS_THAN:
    case LESS_EQUAL_THAN:
    case GREATER_THAN:
    case GREATER_EQUAL_THAN:return true;
    default: return false;
  }
}
std::string_view make_binary_op(token t) {
  switch (t.type) {
    case STAR:return "__binary_op__STAR__";
    case SLASH:return "__binary_op__SLASH__";
    case PLUS:return "__binary_op__PLUS__";
    case MINUS:return "__binary_op__MINUS__";
    case EQUAL:return "__binary_op__EQUAL__";
    case LESS_THAN:return "__binary_op__LESS_THAN__";
    case LESS_EQUAL_THAN:return "__binary_op__LESS_EQUAL_THAN__";
    case GREATER_THAN:return "__binary_op__GREATER_THAN__";
    case GREATER_EQUAL_THAN:return "__binary_op__GREATER_EQUAL_THAN__";
    default: throw parse::error::report_token(t.sv, "", " is not a recognized infix binary operator");
  }
}

ptr make_infix_app(ptr &&left, ptr &&right, token t) {
  return std::make_unique<fun_app>(std::make_unique<fun_app>(std::make_unique<identifier>(make_binary_op(t), t.sv),
                                                             std::move(left)),
                                   std::move(right));
}

ptr make_prefix_app(ptr &&arg, token t) {
  return std::make_unique<fun_app>(std::make_unique<identifier>(make_unary_op(t), t.sv), std::move(arg));
}

ptr make_fun_constr_app(ptr &&f, ptr &&x, bool) {
  if (constructor *c = dynamic_cast<constructor *>(f.get()); c) {
    if (c->arg)throw parse::error::report_token(x->loc, "unexpected ", " this constructors expects no argument");
    c->arg = std::move(x);
    return std::move(f);
  } else return std::make_unique<fun_app>(std::move(f), std::move(x));
}

ptr make_rev_fun_app(ptr &&x, ptr &&f, token t) {
  return std::make_unique<fun_app>(std::move(f), std::move(x));
}

ptr make_fun_app(ptr &&f, ptr &&x, token t) {
  return std::make_unique<fun_app>(std::move(f), std::move(x));
}

ptr parse_e_a(tokenizer &tk) {

  using namespace patterns;
  return parse_fold_r<make_infix_app, tk_sep<EQUAL, NOT_EQUAL>,
                      parse_fold_r<make_infix_app, tk_sep<LESS_THAN, GREATER_THAN, LESS_EQUAL_THAN, GREATER_EQUAL_THAN>,
                                   parse_fold_l<make_fun_app, tk_sep<PIPE_LEFT>,
                                                parse_fold_r<make_rev_fun_app, tk_sep<PIPE_RIGHT>,
                                                             parse_fold_r<make_infix_app, tk_sep<PLUS, MINUS>,
                                                                          parse_fold_r<make_infix_app,
                                                                                       tk_sep<STAR, SLASH>,
                                                                                       parse_fold_unary_l<
                                                                                           make_prefix_app,
                                                                                           tk_sep<PLUS, MINUS>,
                                                                                           parse_fold_r<
                                                                                               make_fun_constr_app,
                                                                                               peek_sep<parse_e_p_first>,
                                                                                               parse_e_p
                                                                                           >>>>>>>>(tk);
  //TODO: create better syntax to express them as a list
}

bool parse_e_p_first(token_type t) {
  return is_in(t, {TRUE, FALSE, LITERAL, IDENTIFIER, CAP_NAME, PARENS_OPEN});
}
ptr parse_e_p(tokenizer &tk) {
  switch (tk.peek()) {
    case TRUE:
    case FALSE:
    case LITERAL: {
      return with_loc(std::make_unique<literal>(ast::literal::parse(tk.pop())), tk.peek_sv());
    }
    case IDENTIFIER: {
      return std::make_unique<identifier>(tk.pop().sv);
    }
    case CAP_NAME: {
      return std::make_unique<constructor>(tk.pop().sv);
    }
      //TODO-someday : implement dot notation
    case PARENS_OPEN: {
      auto loc_start = tk.pop().sv.begin();
      if (tk.peek() == PARENS_CLOSE) {
        auto e = std::make_unique<literal>(std::make_unique<ast::literal::unit>());
        e->loc = itr_sv(loc_start, tk.pop().sv.end());
        return e;
      } else if (tokenizer ahead(tk); ahead.pop(), ahead.peek() == PARENS_CLOSE && is_binary_op(tk.peek())) {
        token t = tk.pop();
        tk.expect_pop(PARENS_CLOSE);
        return std::make_unique<identifier>(make_binary_op(t), t.sv);
      } else {
        auto e = expression::parse(tk);
        tk.expect_peek(PARENS_CLOSE);
        e->loc = itr_sv(loc_start, tk.pop().sv.end());
        return std::move(e);
      }
    }
    default:tk.unexpected_token();
  }
  throw std::runtime_error(AT "How did I end up here");
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
      auto e = std::make_unique<if_then_else>(std::move(condition), std::move(true_branch), std::move(false_branch));
      e->loc = itr_sv(loc_start, e->false_branch->loc.end());
      return e;
    }
    case MATCH: {
      auto loc_start = tk.pop().sv.begin();
      auto what = parse(tk);
      match_with::ptr mtc = std::make_unique<match_with>(std::move(what));
      tk.expect_pop(WITH);
      tk.expect_peek(PIPE);
      while (tk.peek() == PIPE) {
        tk.pop();
        auto m = ast::matcher::parse(tk);
        tk.expect_pop(ARROW);
        auto e = parse(tk);
        mtc->branches.emplace_back();
        mtc->branches.back().pattern = std::move(m);
        mtc->branches.back().result = std::move(e);
      }
      mtc->loc = itr_sv(loc_start, mtc->branches.back().result->loc.end());
      return std::move(mtc);
    }
    case LET: {
      auto loc_start = tk.peek_sv().begin();
      auto d = definition::parse(tk);
      tk.expect_pop(IN);
      auto e = parse(tk);
      return with_loc(std::make_unique<let_in>(std::move(d), std::move(e)), itr_sv(loc_start, e->loc.end()));
    }
    case FUN: {
      auto loc_start = tk.peek_sv().begin();
      tk.expect_pop(FUN);
      std::vector<matcher::ptr> args;
      while (!tk.empty() && tk.peek() != ARROW) {
        args.push_back(matcher::parse(tk));
      }
      tk.expect_pop(ARROW);
      auto body = parse(tk);
      return with_loc(std::make_unique<fun>(std::move(args), std::move(body)), itr_sv(loc_start, body->loc.end()));
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
  using namespace patterns;
  return parse_vec<tuple, &tuple::args, tk_sep<COMMA>, parse_m_2>(tk);
}

ptr parse_m_2(tokenizer &tk) {
  switch (tk.peek()) {
    case CAP_NAME: {
      //TODO: Maybe better structure for loc?
      auto begin_sv = tk.pop().sv;
      if (!parse_m_3_first(tk.peek_full())) return std::make_unique<constructor>(begin_sv);

      auto m = parse_m_3(tk);
      return with_loc(std::make_unique<constructor>(std::move(m), begin_sv),
                      itr_sv(begin_sv.begin(), m->loc.end()));
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
      if (tk.peek() == PARENS_CLOSE)
        return with_loc(std::make_unique<literal>(std::make_unique<ast::literal::unit>()),
                        itr_sv(loc_start, tk.pop().sv.end()));
      auto m = parse_m_1(tk);
      tk.expect_peek(PARENS_CLOSE);
      m->loc = itr_sv(loc_start, tk.pop().sv.end());
      return std::move(m);
    }
    case UNDERSCORE: {
      return with_loc(std::make_unique<ignore>(), tk.pop().sv);
    }
    case IDENTIFIER: {
      return std::make_unique<universal>(tk.pop().sv);
    }
    case TRUE:
    case FALSE:
    case LITERAL: {
      return with_loc(std::make_unique<literal>(ast::literal::parse(tk.pop())), tk.peek_sv());
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

ptr parse(const token &t) {
  switch (t.type) {
    case TRUE:return std::make_unique<boolean>(true);
    case FALSE:return std::make_unique<boolean>(false);
    case LITERAL: {
      if (int64_t n;std::from_chars(t.sv.begin(), t.sv.end(), n).ec == std::errc())return std::make_unique<integer>(n);
      if (t.sv.front() == '\"') {
        //string literal
        std::string_view s = t.sv;
        s.remove_prefix(1);
        s.remove_suffix(1);
        std::string slit;
        for (size_t i = 0; i < s.size(); ++i)
          if (s[i] == '\\') {
            //escape sequences
            if (isdigit(s[i + 1])) {
              //read the number
              size_t j = i + 1, n = 0;
              while (j < s.size() && isdigit(s[j])) {
                n = 10 * n + s[j] - '0';
                ++j;
              }
              if (j - i > 5 || n > 255)
                throw parse::error::report_token(std::string_view(s.data() + i, j - i),
                                                 "Escape sequence ",
                                                 " doesn't fit in single-byte character [0-255].");
              i = j - 1;
              slit.push_back(char(n));
            } else if (chars::is_valid_mnemonic(s[i + 1])) {
              slit.push_back(chars::parse_mnemonic(s[i + 1]));
              ++i;
            } else {
              //error
              throw parse::error::report_token(std::string_view(s.data() + i, 2),
                                               "Escape sequence ",
                                               " is not recognized.");
            }
          } else slit.push_back(s[i]);
        slit.push_back(0);
        while (slit.size() % 8)slit.push_back(0);
        return std::make_unique<string>(std::move(slit));
      }
      //TODO: floating point literal
      throw std::runtime_error(AT "unimplemented floating point literal");
    }
    default: throw std::runtime_error(AT "trying to parse a literal");
  }

}
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
  bool first = true;
  do {
    if (!first)tk.expect_pop(AND);
    if (first)first = false;
    auto loc_start = tk.peek_sv().begin();
    auto m = matcher::parse(tk);
    if (dynamic_cast<matcher::universal *>(m.get()) && tk.peek() != EQUAL) {
      //function alternative definition
      std::vector<matcher::ptr> args;
      //
      //fundef->name.reset(dynamic_cast<matcher::universal *>(m.release()));
      while (tk.peek() != EQUAL) args.push_back(matcher::parse(tk));

      tk.expect_pop(EQUAL);
      auto fun = std::make_unique<expression::fun>(std::move(args), expression::parse(tk));

      fun->loc = itr_sv(m->loc.begin(), fun->body->loc.end());

      defs->defs.emplace_back(std::move(m), std::move(fun));
    } else {
      tk.expect_pop(EQUAL);
      defs->defs.emplace_back(std::move(m), expression::parse(tk));
    }
  } while (tk.peek() == AND);
  defs->loc = itr_sv(loc_start, defs->defs.back().e->loc.end());
  defs->rec = rec;
  return std::move(defs);

}

}

namespace type {
namespace expression {
// Type expressions have the following grammar:
// type-expr = t0
// t0 = t1 | t1 -> t0 ;;
// t1 = t2 | t3 * t2 ;;
// t2 = 'x | (t0) | t2 tf | (t0 [,t0]* )  tf ;;
namespace {

using namespace parse;

ptr parse_t0(tokenizer &tk);
ptr parse_t1(tokenizer &tk);
ptr parse_t2(tokenizer &tk);

auto make_function(ptr &&p1, ptr &&p2, token) { return std::make_unique<function>(std::move(p1), std::move(p2)); };
ptr parse_t0(tokenizer &tk) {
  using namespace patterns;
  return parse_fold_l<make_function, tk_sep<ARROW>, parse_t1>(tk);
}
ptr parse_t1(tokenizer &tk) {
  using namespace patterns;
  return parse_vec<product, &product::ts, tk_sep<STAR>, parse_t2>(tk);
}
ptr parse_t2(tokenizer &tk) {
  tk.expect_peek_any_of({PARENS_OPEN, IDENTIFIER});
  if (tk.peek() == PARENS_OPEN) {
    tk.pop();
    ptr p = parse_t0(tk);
    tk.expect_peek_any_of({PARENS_CLOSE, COMMA});
    if (tk.pop().type == PARENS_CLOSE) {
      while (tk.peek() == IDENTIFIER) {
        p = std::make_unique<application>(std::move(p), std::make_unique<identifier>(tk.pop().sv));
      }
      return p;
    } else { // == COMMA
      auto args = std::make_unique<tuple>(std::move(p));
      args->ts.push_back(parse_t0(tk));
      tk.expect_peek_any_of({PARENS_CLOSE, COMMA});
      while (tk.peek() == COMMA) {
        tk.pop();
        args->ts.push_back(parse_t0(tk));
        tk.expect_peek_any_of({PARENS_CLOSE, COMMA});
      }
      tk.expect_pop(PARENS_CLOSE);
      //now we NEED to have a constructor
      tk.expect_peek(IDENTIFIER); //If not, bad boi
      p = std::make_unique<application>(std::move(args), std::make_unique<identifier>(tk.pop().sv));
      while (tk.peek() == IDENTIFIER) {
        p = std::make_unique<application>(std::move(p), std::make_unique<identifier>(tk.pop().sv));
      }
      return p;
    }
  } else {
    tk.expect_peek(IDENTIFIER);
    ptr p = std::make_unique<identifier>(tk.pop().sv);
    while (tk.peek() == IDENTIFIER) {
      p = std::make_unique<application>(std::move(p), std::make_unique<identifier>(tk.pop().sv));
    }
    return p;
  }

}
}
/*
bool parse_i_first(token_type);
ptr parse_c(tokenizer &tk);
ptr parse_p(tokenizer &tk);
ptr parse_f(tokenizer &tk);
ptr parse_t(tokenizer &tk);

bool parse_i_first(token_type t) {
  return is_in(t, {PARENS_OPEN, IDENTIFIER});
}
ptr parse_i(tokenizer &tk) {
  switch (tk.peek()) {
    case PARENS_OPEN: {
      auto loc_start = tk.peek_sv().begin();
      tk.expect_pop(PARENS_OPEN);
      ptr t = parse(tk);
      tk.expect_peek(PARENS_CLOSE);
      t->loc = itr_sv(loc_start, tk.pop().sv.end());
      return t;
    }
    case IDENTIFIER: {
      return std::make_unique<identifier>(tk.pop().sv);
    }
    default:tk.unexpected_token();
  }
  THROW_UNIMPLEMENTED;
}
ptr parse_c(tokenizer &tk) {
  ptr x = parse_i(tk);
  while (parse_i_first(tk.peek())) {
    ptr f = parse_i(tk);
    x = std::make_unique<constr>(std::move(x), std::move(f));
  }
  return x;
}

ptr parse_p(tokenizer &tk) {
  using namespace patterns;
  return parse_vec<product, &product::ts, tk_sep<STAR>, parse_c>(tk);
}
ptr make_function(ptr &&a, ptr &&b, token t) {
  return std::make_unique<function>(std::move(a), std::move(b));
}
ptr parse_f(tokenizer &tk) {
  using namespace patterns;
  return parse_fold_l<make_function, tk_sep<ARROW>, parse_p>(tk);
}

ptr parse_t(tokenizer &tk) {

  //return patterns::parse_vec<tuple,&tuple::ts,parse_f,patterns::tk_sep<COMMA>>(tk);

  //TODO: compress
  ptr t = parse_f(tk);
  if (tk.peek() != COMMA)return t;
  tuple::ptr ts = std::make_unique<tuple>(std::move(t));
  while (tk.peek() == COMMA) {
    tk.expect_pop(COMMA);
    ts->ts.push_back(parse_f(tk));
  }
  // ts->set_loc();
  return ts;
}

}
*/
ptr parse(tokenizer &tk) {
  return parse_t0(tk);
}
}

namespace definition {
ptr parse(tokenizer &tk) {
  auto loc_start = tk.peek_sv().begin();
  tk.expect_pop(TYPE);
  bool nonrec = false;
  if (tk.peek() == NONREC) {
    nonrec = true;
    tk.expect_pop(NONREC);
  }
  ptr defs = std::make_unique<t>();
  bool first = true;
  do {
    if (!first)tk.expect_pop(AND);
    if (first)first = false;
    auto single_loc_start = tk.peek_sv().begin();
    auto m = expression::parse(tk);
    std::vector<param::ptr> params;
    if (dynamic_cast<expression::application *>(m.get())) {
      //TODO: def->args := m.x, m := m.f
      std::unique_ptr<expression::application> c(dynamic_cast<expression::application *>(m.release()));
      m = std::move(c->f);
      if (dynamic_cast<expression::identifier *>(c->x.get())) {
        params.push_back(std::make_unique<param>(dynamic_cast<expression::identifier *>(c->x.get())->name));
      } else if (dynamic_cast<expression::tuple *>(c->x.get())) {
        for (expression::ptr &t : dynamic_cast<expression::tuple *>(c->x.get())->ts) {
          if (expression::identifier *i = dynamic_cast<expression::identifier *>(m.get()); i == nullptr) {
            THROW_UNIMPLEMENTED
            //TODO: throw exception "not allowed"
          } else params.push_back(std::make_unique<param>(i->name));
        }
      } else {
        THROW_UNIMPLEMENTED
        //TODO: throw exception "not allowed"
      }
    }
    if (dynamic_cast<expression::identifier *>(m.get()) == nullptr) {
      //TODO: throw some error
      THROW_UNIMPLEMENTED
    }
    std::string_view def_name = dynamic_cast<expression::identifier *>(m.get())->name;
    //we got the name, now let's get the definition
    tk.expect_pop(EQUAL);
    if (tk.peek() == PIPE) {
      //variant
      single_variant::ptr def = std::make_unique<single_variant>();
      def->params = std::move(params);
      def->name = def_name;

      while (tk.peek() == PIPE) {
        tk.expect_pop(PIPE);
        tk.expect_peek(CAP_NAME);
        def->variants.emplace_back();
        def->variants.back().name = tk.pop().sv;
        if (tk.peek() == OF) {
          tk.expect_pop(OF);
          def->variants.back().args.push_back(expression::parse_t2(tk));
          while(tk.peek() == STAR){
            tk.expect_pop(STAR);
            def->variants.back().args.push_back(expression::parse_t2(tk));
          }
          if(tk.peek()==ARROW){
            THROW_UNIMPLEMENTED //TODO: throw syntax error as this //type t = | A of int -> int ;; is not allowed
          }
        }
      }
      def->loc = itr_sv(single_loc_start,def->variants.back().args.empty()  ? def->variants.back().name.end() : def->variants.back().args.back()->loc.end());
      defs->defs.push_back(std::move(def));
    } else {
      //texpr
      single_texpr::ptr def = std::make_unique<single_texpr>();
      def->params = std::move(params);
      def->name = def_name;
      def->type = expression::parse(tk);
      def->loc = itr_sv(loc_start, def->type->loc.end());
      defs->defs.push_back(std::move(def));
    }

  } while (tk.peek() == AND);
  defs->loc = itr_sv(loc_start, defs->defs.back()->loc.end());
  defs->nonrec = nonrec;
  return std::move(defs);

}

}

}

}


