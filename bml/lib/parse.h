#ifndef COMPILERS_BML_LIB_PARSE_H_
#define COMPILERS_BML_LIB_PARSE_H_

#include <ast.h>
#include <array>
#include <cassert>
#include <algorithm>
#include <charconv>
#include <iostream>
#include "util.h"
enum token_type { LITERAL, IDENTIFIER, CAP_NAME, PARENS_OPEN, PARENS_CLOSE, EQUAL, PIPE, ARROW, PLUS, MINUS, EOC, LET, REC, IN, AND, WITH, MATCH, COMMA, COLON, SEMICOLON, DOT, IF, THEN, ELSE, TRUE, FALSE, UNDERSCORE };
namespace {
typedef std::pair<std::string_view, token_type> st;
}
constexpr auto tokens_map = make_array(
    st{"(", token_type::PARENS_OPEN},
    st{")", token_type::PARENS_CLOSE},
    st{"=", EQUAL}, st{"|", PIPE},
    st{"->", ARROW}, st{"-", MINUS},
    st{"+", PLUS}, st{";;", EOC},
    st{"let", LET}, st{"rec", REC},
    st{"in", IN}, st{"and", AND},
    st{"with", WITH}, st{"match", MATCH},
    st{",", COMMA}, st{":", COLON},
    st{";", SEMICOLON}, st{".", DOT},
    st{"if", IF}, st{"then", THEN}, st{"else", ELSE},
    st{"true", TRUE}, st{"false", FALSE}, st{"_", UNDERSCORE});

struct token {

  std::string_view sv;
  token_type type;
  std::string to_string() const {
    switch (type) {
      case LITERAL:return std::string(sv);
      case IDENTIFIER:return std::string("<").append(sv).append(">");
      case CAP_NAME:return std::string("[").append(sv).append("]");
      case PARENS_OPEN:return "PARENS_OPEN";
      case PARENS_CLOSE:return "PARENS_CLOSE";
      case EQUAL:return "EQUAL";
      case PIPE:return "PIPE";
      case ARROW:return "ARROW";
      case PLUS:return "PLUS";
      case MINUS:return "MINUS";
      case EOC:return "EOC";
      case LET:return "LET";
      case REC:return "REC";
      case IN:return "IN";
      case AND:return "AND";
      case WITH:return "WITH";
      case MATCH:return "MATCH";
      case COMMA:return "COMMA";
      case COLON:return "COLON";
      case SEMICOLON:return "SEMICOLON";
      case DOT:return "DOT";
      case IF:return "IF";
      case THEN:return "THEN";
      case ELSE:return "ELSE";
      case TRUE:return "TRUE";
      case FALSE:return "FALSE";
    }
    return std::string("unimplemented (").append(sv).append(")");
    //assert(false);
  }
};

std::vector<token> tokenize(std::string_view s);
/*

 expressions

 E -> Es | IF E THEN E ELSE E |  MATCH E WITH ( | E  -> E )*+ | LET ... |
 Es -> Et | Et SEMICOLON Es                 i.e. a list of E2s, any associativity
 Et -> Ef COMMA Et | Ef
 Ef -> Ef Ep | Ep                   i.e. a list of E4s, left associativity
 Ep -> () | ( E ) | Literal | Ex
 Ex -> Modulename DOT Ex | Identififer | Constructor

 matchers

 M -> M1
 M1 -> M2 | M2 COMMA M1
 M2 -> M3 | Constructor | Constructor M3
 M3 -> () | _ | binder | Literal | (M)


 M -> _ | Identifier | Literal | Constructor M | Tuple of M


 */
#define EXPECT_TK( tk )  if (begin == end) throw std::runtime_error("unexpected EOF, expected \"" #tk "\""); if (begin->type != tk ) throw std::runtime_error(formatter() << "expected \"" #tk "\", found " << begin->to_string() >> formatter::to_str);
#define EXPECT_SOME( ) if (begin == end) throw std::runtime_error("unexpected EOF");
namespace ast {

template<typename TokenIt>
std::pair<expression::ptr, TokenIt> parse_expression(TokenIt begin, TokenIt end);
template<typename TokenIt>
std::pair<matcher::ptr, TokenIt> parse_matcher(TokenIt begin, TokenIt end);

namespace {

literal::ptr parse_literal(const token &t) {
  switch (t.type) {
    case TRUE:return std::make_unique<bool_literal>(true, t.sv);
    case FALSE:return std::make_unique<bool_literal>(false, t.sv);
    case LITERAL: {
      if (int64_t n;std::from_chars(t.sv.begin(), t.sv.end(), n).ec == std::errc())return std::make_unique<int_literal>(n, t.sv);
      //TODO: floating point literal, string literal
      throw std::runtime_error("unimplemented floating point literal, string literal");
    }
    default: throw std::runtime_error("trying to parse a literal");
  }

}

template<typename TokenIt>
std::pair<expression::ptr, TokenIt> parse_e_s(TokenIt begin, TokenIt end);
template<typename TokenIt>
std::pair<expression::ptr, TokenIt> parse_e_t(TokenIt begin, TokenIt end);
template<typename TokenIt>
std::pair<expression::ptr, TokenIt> parse_e_f(TokenIt begin, TokenIt end);
template<typename TokenIt>
std::pair<expression::ptr, TokenIt> parse_e_p(TokenIt begin, TokenIt end);
bool parse_e_p_first(const token &t);
literal::ptr parse_literal(const token &t);

template<typename TokenIt>
std::pair<expression::ptr, TokenIt> parse_e_s(TokenIt begin, TokenIt end) {
#ifdef DEBUG
  std::cerr << "parse_e_1: " ;
  for(TokenIt t = begin; t!=end;++t)std::cerr <<" "<<t->to_string();
  std::cerr<<std::endl;
#endif
  auto[e2, nd] = parse_e_t(begin, end);
  begin = nd;
  while (begin != end && begin->type == SEMICOLON) {
    ++begin;
    auto[ne2, nd] = parse_e_t(begin, end);
    begin = nd;
    e2 = std::make_unique<seq>(std::move(e2), std::move(ne2), itr_sv(e2->loc.begin(), ne2->loc.end()));
  }
  return {std::move(e2), begin};
}

template<typename TokenIt>
std::pair<expression::ptr, TokenIt> parse_e_t(TokenIt begin, TokenIt end) {
#ifdef DEBUG
  std::cerr << "parse_e_2: " ;
    for(TokenIt t = begin; t!=end;++t)std::cerr <<" "<<t->to_string();
    std::cerr<<std::endl;
#endif
  auto[e3, nd] = parse_e_f(begin, end);
  begin = nd;
  if (begin == end || begin->type != COMMA)return {std::move(e3), begin};
  auto e2 = std::make_unique<build_tuple>(std::string_view());
  e2->args.emplace_back(std::move(e3));
  while (begin != end && begin->type == COMMA) {
    ++begin;
    auto[e3, nd] = parse_e_f(begin, end);
    begin = nd;
    e2->args.emplace_back(std::move(e3));
  }
  e2->loc = itr_sv(e2->args.front()->loc.begin(), e2->args.back()->loc.end());
  return {std::move(e2), begin};
}

template<typename TokenIt>
std::pair<expression::ptr, TokenIt> parse_e_f(TokenIt begin, TokenIt end) {
#ifdef DEBUG
  std::cerr << "parse_e_3: " ;
    for(TokenIt t = begin; t!=end;++t)std::cerr <<" "<<t->to_string();
    std::cerr<<std::endl;
#endif
  auto[e4, nd] = parse_e_p(begin, end);
  begin = nd;
  while (begin != end && parse_e_p_first(*begin)) {
    auto[ne4, nd] = parse_e_p(begin, end);
    begin = nd;
    e4 = std::make_unique<fun_app>(std::move(e4), std::move(ne4), itr_sv(e4->loc.begin(), ne4->loc.end()));
  }
  return {std::move(e4), begin};
}

bool is_integer(std::string_view s) {
  return !s.empty() && (s.front() == '-' || ::isdigit(s.front())) && std::all_of(s.begin() + 1, s.end(), ::isdigit);
}

bool parse_e_p_first(const token &t) {
  return is_in(t.type, {TRUE, FALSE, LITERAL, IDENTIFIER, CAP_NAME, PARENS_OPEN});
}
bool parse_e_first(const token &t) {
  return is_in(t.type, {TRUE, FALSE, LITERAL, IDENTIFIER, CAP_NAME, PARENS_OPEN, IF, MATCH, LET});
}

template<typename TokenIt>
std::pair<expression::ptr, TokenIt> parse_e_p(TokenIt begin, TokenIt end) {
#ifdef DEBUG
  std::cerr << "parse_e_4: " ;
    for(TokenIt t = begin; t!=end;++t)std::cerr <<" "<<t->to_string();
    std::cerr<<std::endl;
#endif
  if (begin == end) throw std::runtime_error("unexpected EOF");
  switch (begin->type) {
    case TRUE:
    case FALSE:
    case LITERAL: {
      return {parse_literal(*begin), ++begin};
    }
    case IDENTIFIER:
    case CAP_NAME: {
      return {std::make_unique<identifier>(begin->sv), ++begin};
    }
      //TODO-someday : implement dot notation
    case PARENS_OPEN: {
      auto loc_start = begin->sv.begin();
      ++begin;
      if (begin == end) throw std::runtime_error("unexpected EOF; did you forget a  \")\" ?");
      if (begin->type != PARENS_CLOSE) {
        auto[e, nd] = parse_expression(begin, end);
        if (nd == end) throw std::runtime_error("unexpected EOF; did you forget a  \")\" ?");
        if (nd->type != PARENS_CLOSE)throw std::runtime_error("expected \")\"");
        //IDEA: enlarge
        e->loc = itr_sv(loc_start, nd->sv.end());
        ++nd;
        return {std::move(e), nd};
      } else {
        return {std::make_unique<unit_literal>(itr_sv(loc_start, begin->sv.end())), ++begin};
      }
    }
    default:throw std::runtime_error("expected either ( or identifier");
  }
}

}

namespace {

template<typename TokenIt>
std::pair<matcher::ptr, TokenIt> parse_m_1(TokenIt begin, TokenIt end);
template<typename TokenIt>
std::pair<matcher::ptr, TokenIt> parse_m_2(TokenIt begin, TokenIt end);
template<typename TokenIt>
std::pair<matcher::ptr, TokenIt> parse_m_3(TokenIt begin, TokenIt end);
bool parse_m_3_first(const token &t);

template<typename TokenIt>
std::pair<matcher::ptr, TokenIt> parse_m_1(TokenIt begin, TokenIt end) {
  auto[m2, nd] = parse_m_2(begin, end);
  begin = nd;
  if (begin == end || begin->type != COMMA)return {std::move(m2), begin};
  auto m1 = std::make_unique<tuple_matcher>(std::string_view());
  m1->args.emplace_back(std::move(m2));
  while (begin != end && begin->type == COMMA) {
    ++begin;
    auto[m2, nd] = parse_m_2(begin, end);
    begin = nd;
    m1->args.emplace_back(std::move(m2));
  }
  m1->loc = itr_sv(m1->args.front()->loc.begin(), m1->args.back()->loc.end());
  return {std::move(m1), begin};
}
template<typename TokenIt>
std::pair<matcher::ptr, TokenIt> parse_m_2(TokenIt begin, TokenIt end) {
  if (begin == end) throw std::runtime_error("unexpected EOF");
  switch (begin->type) {
    case CAP_NAME: {
      auto begin_sv = begin->sv.begin();
      ++begin;
      if (begin == end || !parse_m_3_first(*begin)) return {std::make_unique<constructor_matcher>(begin_sv), begin};

      auto[m, nd] = parse_m_3(begin, end);
      return {std::make_unique<constructor_matcher>(std::move(m), itr_sv(begin_sv, m->loc.end()), begin_sv), nd};
    }
    default: { return parse_m_3(begin, end); }
  }
}

bool parse_m_3_first(const token &t) {
  return is_in(t.type, {TRUE, FALSE, LITERAL, IDENTIFIER, PARENS_OPEN, UNDERSCORE});
}
bool parse_m_1_first(const token &t) {
  return is_in(t.type, {TRUE, FALSE, LITERAL, IDENTIFIER, PARENS_OPEN, CAP_NAME, UNDERSCORE});
}
template<typename TokenIt>
std::pair<matcher::ptr, TokenIt> parse_m_3(TokenIt begin, TokenIt end) {
  if (begin == end) throw std::runtime_error("unexpected EOF");
  switch (begin->type) {
    case PARENS_OPEN: {
      auto loc_start = begin->sv.begin();
      ++begin;
      if (begin == end) throw std::runtime_error("unexpected EOF - expected )");
      if (begin->type == PARENS_CLOSE)return {std::make_unique<literal_matcher>(std::make_unique<unit_literal>(itr_sv(loc_start, begin->sv.end())), itr_sv(loc_start, begin->sv.end())), ++begin};
      auto[m, nd] = parse_m_1(begin, end);
      if (nd == end) throw std::runtime_error("unexpected EOF - expected )");
      if (nd->type != PARENS_CLOSE) throw std::runtime_error("expected )");
      m->loc = itr_sv(loc_start, nd->sv.end());
      ++nd;
      return {std::move(m), nd};
    }
    case UNDERSCORE: {
      return {std::make_unique<anonymous_universal_matcher>(begin->sv), ++begin};
    }
    case IDENTIFIER: {
      return {std::make_unique<universal_matcher>(begin->sv), ++begin};
    }
    case TRUE: case FALSE: case LITERAL: {
      return {std::make_unique<literal_matcher>(parse_literal(*begin), begin->sv), ++begin};
    }
    default: {
      throw std::runtime_error("unexpected_token");
    }
  }

}

}

template<typename TokenIt>
std::pair<definition::ptr, TokenIt> parse_definition(TokenIt begin, TokenIt end){
  EXPECT_TK(LET);
  auto loc_start = begin->sv.begin();
  ++begin;
  bool rec = false;
  EXPECT_SOME();
  if(begin->type==REC){
    rec = true;
    ++begin;
    EXPECT_SOME();
  }
  definition::ptr defs = std::make_unique<definition>();
  do{
    auto loc_start = begin->sv.begin();
    auto [m,nd] = parse_matcher(begin,end);
    begin = nd;
    if (dynamic_cast<universal_matcher*>(m.get()) &&  begin!=end && begin->type!=EQUAL){
      //function definition
      function_definition::ptr fundef = std::make_unique<function_definition>();
      fundef->name.reset(dynamic_cast<universal_matcher*>(m.release()));
      while (begin->type!=EQUAL){
        auto [m,nd] = parse_matcher(begin,end);
        begin = nd;
        fundef->args.push_back(std::move(m));
      }
      EXPECT_TK(EQUAL);++begin;
      auto [e,nd] = parse_expression(begin,end);
      begin = nd;
      fundef->body = std::move(e);
      fundef->loc = itr_sv(fundef->name->loc.begin(),fundef->body->loc.end());
      defs->defs.push_back(std::move(fundef));
    }else{
      //value binding
      EXPECT_TK(EQUAL);++begin;
      auto [e,nd] = parse_expression(begin,end);
      begin = nd;
      defs->defs.push_back( std::move(std::make_unique<value_definition>(std::move(m),std::move(e),itr_sv(loc_start,e->loc.end())) ));
    }
  }while (begin!=end && begin->type==AND && ++begin!=end);
  defs->loc = itr_sv(loc_start,defs->defs.back()->loc.end());
  return {std::move(defs),begin};

}


template<typename TokenIt>
std::pair<matcher::ptr, TokenIt> parse_matcher(TokenIt begin, TokenIt end) {
  return parse_m_1(begin, end);
}


template<typename TokenIt>
std::pair<expression::ptr, TokenIt> parse_expression(TokenIt begin, TokenIt end) {
#ifdef DEBUG
  std::cerr << "parse_expression: " ;
    for(TokenIt t = begin; t!=end;++t)std::cerr <<" "<<t->to_string();
    std::cerr<<std::endl;
#endif
  static_assert(std::is_same_v<typename std::iterator_traits<TokenIt>::value_type, token>, "This should have token");
  if (begin == end) throw std::runtime_error("unexpected EOF");
  switch (begin->type) {
    case IF: {
      auto loc_start = begin->sv.begin();
      ++begin;
      auto[condition, nb] = parse_expression(begin, end);
      if (nb == end) throw std::runtime_error("unexpected EOF, expected \"then\"");
      if (nb->type != THEN) throw std::runtime_error(formatter() << "expected \"then\", found " << nb->to_string() >> formatter::to_str);
      ++nb;
      auto[true_branch, nnb] = parse_expression(nb, end);
      if (nnb == end) throw std::runtime_error("unexpected EOF, expected \"else\"");
      if (nnb->type != ELSE) throw std::runtime_error(formatter() << "expected \"else\", found " << nnb->to_string() >> formatter::to_str);
      ++nnb;
      auto[false_branch, nnnb] = parse_expression(nnb, end);
      auto loc = std::string_view(loc_start, false_branch->loc.end() - loc_start);
      return {std::make_unique<if_then_else>(std::move(condition), std::move(true_branch), std::move(false_branch), loc), nnnb};
    }
    case MATCH: {
      auto loc_start = begin->sv.begin();
      ++begin;
      auto what_nd = parse_expression(begin, end);
      begin = what_nd.second;
      match_with::ptr mtc = std::make_unique<match_with>(std::move(what_nd.first), std::string_view());
      if (begin == end) throw std::runtime_error("unexpected EOF, expected \"with\"");
      if (begin->type != WITH) throw std::runtime_error(formatter() << "expected \"with\", found " << begin->to_string() >> formatter::to_str);
      ++begin;
      if (begin == end) throw std::runtime_error("unexpected EOF, expected \"|\"");
      if (begin->type != PIPE) throw std::runtime_error(formatter() << "expected \"|\", found " << begin->to_string() >> formatter::to_str);
      while (begin != end && begin->type == PIPE) {
        ++begin;
        auto[m, nd] = parse_m_1(begin, end);
        begin = nd;
        if (begin == end) throw std::runtime_error("unexpected EOF, expected \"->\"");
        if (begin->type != ARROW) throw std::runtime_error(formatter() << "expected \"->\", found " << begin->to_string() >> formatter::to_str);
        ++begin;
        auto[e, ndd] = parse_expression(begin, end);
        begin = ndd;
        mtc->branches.push_back({.pattern=std::move(m), .result=std::move(e)});
      }
      mtc->loc = itr_sv(loc_start, mtc->branches.back().result->loc.end());
      return {std::move(mtc), begin};
    }
    case LET:{
      //auto [d,nd] = parse_definition;

      throw std::runtime_error("unimplemented");
    }
    default:return parse_e_s(begin, end);
  }
}

}

#endif //COMPILERS_BML_LIB_PARSE_H_
