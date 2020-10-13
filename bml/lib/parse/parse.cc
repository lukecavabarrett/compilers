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

template<token_type sep>
struct tk_sep {
  static bool has_sep(const tokenizer &tk) { return tk.peek() == sep; }
  static void consume_sep(tokenizer &tk) { tk.expect_pop(sep); }
};

template<auto F>
struct peek_sep {
  static bool has_sep(const tokenizer &tk) { return F(tk.peek()); }
  static void consume_sep(tokenizer &tk) {}
};

template<typename vec_type, auto vec_ptr, auto sub_method, typename sep>
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

template<auto make_lr_type, auto sub_method, typename sep>
std::invoke_result_t<decltype(sub_method), tokenizer &> parse_fold_r(tokenizer &tk) {
  auto l = sub_method(tk);
  while (sep::has_sep(tk)) {
    sep::consume_sep(tk);
    l = make_lr_type(std::move(l), sub_method(tk));
  }
  return l;
}

template<auto make_lr_type, auto sub_method, typename sep>
std::invoke_result_t<decltype(sub_method), tokenizer &> parse_fold_l(tokenizer &tk) {
  auto l = sub_method(tk);
  if (!sep::has_sep(tk))return l;
  sep::consume_sep(tk);
  return make_lr_type(std::move(l), parse_fold_l<make_lr_type, sub_method, sep>(tk));
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
  if (head.type != t) {
    throw error::expected_token_found_another(token_type_to_string(t), head.sv);
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
ptr parse_e_f(tokenizer &tk);
ptr parse_e_p(tokenizer &tk);
bool parse_e_p_first(token_type);

ptr parse_e_s(tokenizer &tk) {
  using namespace patterns;
  return parse_fold_r<std::make_unique<seq, ptr &&, ptr &&>, parse_e_t, tk_sep<SEMICOLON>>(tk);
}

ptr parse_e_t(tokenizer &tk) {
  using namespace patterns;
  return parse_vec<build_tuple, &build_tuple::args, parse_e_f, tk_sep<COMMA>>(tk);
}

ptr parse_e_f(tokenizer &tk) {
  using namespace patterns;
  return parse_fold_r<std::make_unique<fun_app, ptr &&, ptr &&>, parse_e_p, peek_sep<parse_e_p_first>>(tk);
}

bool parse_e_p_first(token_type t) {
  return is_in(t, {TRUE, FALSE, LITERAL, IDENTIFIER, CAP_NAME, PARENS_OPEN});
}
ptr parse_e_p(tokenizer &tk) {
  switch (tk.peek()) {
    case TRUE:
    case FALSE:
    case LITERAL: {
      return std::make_unique<literal>(ast::literal::parse(tk.pop()));
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
      if (tk.peek() != PARENS_CLOSE) {
        auto e = expression::parse(tk);
        tk.expect_peek(PARENS_CLOSE);
        e->loc = itr_sv(loc_start, tk.pop().sv.end());
        return std::move(e);
      } else {
        auto e = std::make_unique<literal>(std::make_unique<ast::literal::unit>());
        e->loc = itr_sv(loc_start, tk.pop().sv.end());
        return e;
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
        mtc->branches.back().pattern=std::move(m);
        mtc->branches.back().result=std::move(e);
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
  return parse_vec<tuple_matcher, &tuple_matcher::args, parse_m_2, tk_sep<COMMA>>(tk);
}

ptr parse_m_2(tokenizer &tk) {
  switch (tk.peek()) {
    case CAP_NAME: {
      //TODO: Maybe better structure for loc?
      auto begin_sv = tk.pop().sv;
      if (!parse_m_3_first(tk.peek_full())) return std::make_unique<constructor_matcher>(begin_sv);

      auto m = parse_m_3(tk);
      return with_loc(std::make_unique<constructor_matcher>(std::move(m),begin_sv), itr_sv(begin_sv.begin(), m->loc.end()));
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
      if (tk.peek() == PARENS_CLOSE)return with_loc(std::make_unique<literal_matcher>(std::make_unique<ast::literal::unit>()), itr_sv(loc_start, tk.pop().sv.end()));
      auto m = parse_m_1(tk);
      tk.expect_peek(PARENS_CLOSE);
      m->loc = itr_sv(loc_start, tk.pop().sv.end());
      return std::move(m);
    }
    case UNDERSCORE: {
      return with_loc(std::make_unique<anonymous_universal_matcher>(), tk.pop().sv);
    }
    case IDENTIFIER: {
      return std::make_unique<universal_matcher>(tk.pop().sv);
    }
    case TRUE:
    case FALSE:
    case LITERAL: {
      return with_loc(std::make_unique<literal_matcher>(ast::literal::parse(tk.pop())), tk.peek_sv());
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
      //TODO: floating point literal, string literal
      throw std::runtime_error(AT "unimplemented floating point literal, string literal");
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
      defs->defs.push_back(with_loc(std::make_unique<value>(std::move(m), std::move(e)), itr_sv(loc_start, e->loc.end())));
    }
  } while (tk.peek() == AND);
  defs->loc = itr_sv(loc_start, defs->defs.back()->loc.end());
  defs->rec = rec;
  return std::move(defs);

}

}

namespace type {
namespace expression {

namespace {

using namespace parse;

ptr parse_i(tokenizer &tk);
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
  return parse_vec<product, &product::ts, parse_c, tk_sep<STAR>>(tk);
}
ptr parse_f(tokenizer &tk) {
  return patterns::parse_fold_l<std::make_unique<function, ptr &&, ptr &&>, parse_p, patterns::tk_sep<ARROW>>(tk);
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

ptr parse(tokenizer &tk) {
  return parse_t(tk);
}
}

namespace definition {
ptr parse(tokenizer &tk) {
  auto loc_start = tk.peek_sv().begin();
  tk.expect_pop(TYPE);
  bool nonrec = false;
  if (tk.peek() == NONREC) {
    nonrec = true;
    tk.expect_pop(REC);
  }
  ptr defs = std::make_unique<t>();
  bool first = true;
  do {
    if (!first)tk.expect_pop(AND);
    if (first)first = false;
    auto loc_start = tk.peek_sv().begin();
    auto m = expression::parse(tk);
    std::vector<param::ptr> params;
    if (dynamic_cast<expression::constr *>(m.get())) {
      //TODO: def->args := m.x, m := m.f
      std::unique_ptr<expression::constr> c(dynamic_cast<expression::constr *>(m.release()));
      m.reset(c->f.release());
      if (dynamic_cast<expression::identifier *>(c->x.get())) {
        params.push_back(std::make_unique<param>(dynamic_cast<expression::identifier *>(c->x.get())->name));
      } else if (dynamic_cast<expression::tuple *>(c->x.get())) {
        for (expression::ptr &t : dynamic_cast<expression::tuple *>(c->x.get())->ts) {
          if (expression::identifier *i = dynamic_cast<expression::identifier *>(m.get()); i == nullptr) {
            THROW_UNIMPLEMENTED
          } else params.push_back(std::make_unique<param>(i->name));
        }
      } else {
        THROW_UNIMPLEMENTED
      }
    }
    if (dynamic_cast<expression::identifier *>(m.get()) == nullptr) {
      //TODO: throw some error
      THROW_UNIMPLEMENTED
    }

    std::string_view def_name = dynamic_cast<expression::identifier *>(m.get())->name;
    tk.expect_pop(EQUAL);
    if (tk.peek() == PIPE) {
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
          def->variants.back().type = expression::parse(tk);
        }
      }
      def->loc = itr_sv(loc_start, def->variants.back().type.operator bool() ? def->variants.back().type->loc.end() : def->variants.back().name.end());
      defs->defs.push_back(std::move(def));

    } else {
      single_texpr::ptr def = std::make_unique<single_texpr>();
      def->params = std::move(params);
      def->name = def_name;
      def->type = expression::parse(tk);
      def->loc = itr_sv(loc_start, def->type->loc.end());
      defs->defs.push_back(std::move(def));
    }


    /*if (dynamic_cast<matcher::universal_matcher *>(m.get()) && tk.peek() != EQUAL) {
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
    }*/
  } while (tk.peek() == AND);
  defs->loc = itr_sv(loc_start, defs->defs.back()->loc.end());
  defs->nonrec = nonrec;
  return std::move(defs);

}

}

}

}


