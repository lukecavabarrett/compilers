#ifndef COMPILERS_BML_LIB_PARSE_H_
#define COMPILERS_BML_LIB_PARSE_H_

#include <ast.h>
#include <array>
#include <cassert>
#include <algorithm>
#include <charconv>
#include <iostream>
#include "util.h"
enum token_type { FREE, PARENS_OPEN, PARENS_CLOSE, EQUAL, PIPE, ARROW, PLUS, MINUS, EOC, LET, REC, IN, AND, WITH, MATCH, COMMA, COLON, SEMICOLON, DOT, IF, THEN, ELSE, TRUE, FALSE };
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
    st{"true", TRUE}, st{"false", FALSE});

struct token {

  std::string_view sv;
  token_type type;
  std::string to_string() const {
    switch (type) {
      case FREE:return std::string("<").append(sv).append(">");
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
 E -> E1 | IF E THEN E ELSE E |  MATCH ... | LET ... |
 E1 -> E2 | E2 SEMICOLON E1                 i.e. a list of E2s, any associativity
 E2 -> E3 COMMA E2 | E3
 E3 -> E3 E4 | E4                   i.e. a list of E4s, left associativity
 E4 -> () | ( E ) | Literal | Identifier


 */

namespace ast {

template<typename TokenIt>
std::pair<expression::ptr, TokenIt> parse_expression(TokenIt begin, TokenIt end);
namespace {

template<typename TokenIt>
std::pair<expression::ptr, TokenIt> parse_e_1(TokenIt begin, TokenIt end);
template<typename TokenIt>
std::pair<expression::ptr, TokenIt> parse_e_2(TokenIt begin, TokenIt end);
template<typename TokenIt>
std::pair<expression::ptr, TokenIt> parse_e_3(TokenIt begin, TokenIt end);
template<typename TokenIt>
std::pair<expression::ptr, TokenIt> parse_e_4(TokenIt begin, TokenIt end);

template<typename TokenIt>
std::pair<expression::ptr, TokenIt> parse_e_1(TokenIt begin, TokenIt end) {
#ifdef DEBUG
  std::cerr << "parse_e_1: " ;
  for(TokenIt t = begin; t!=end;++t)std::cerr <<" "<<t->to_string();
  std::cerr<<std::endl;
#endif
  auto[e2, nd] = parse_e_2(begin, end);
  begin = nd;
  while (begin != end && begin->type == SEMICOLON) {
    ++begin;
    auto[ne2, nd] = parse_e_2(begin, end);
    begin = nd;
    e2 = std::make_unique<seq>(std::move(e2), std::move(ne2), itr_sv(e2->loc.begin(), ne2->loc.end()));
  }
  return {std::move(e2), begin};
}

template<typename TokenIt>
std::pair<expression::ptr, TokenIt> parse_e_2(TokenIt begin, TokenIt end) {
#ifdef DEBUG
  std::cerr << "parse_e_2: " ;
    for(TokenIt t = begin; t!=end;++t)std::cerr <<" "<<t->to_string();
    std::cerr<<std::endl;
#endif
  auto[e3, nd] = parse_e_3(begin, end);
  begin = nd;
  if (begin == end || begin->type != COMMA)return {std::move(e3), begin};
  auto e2 = std::make_unique<build_tuple>(std::string_view());
  e2->args.emplace_back(std::move(e3));
  while (begin != end && begin->type == SEMICOLON) {
    ++begin;
    auto[e3, nd] = parse_e_3(begin, end);
    begin = nd;
    e2->args.emplace_back(std::move(e3));
  }
  e2->loc = itr_sv(e2->args.front()->loc.begin(), e2->args.back()->loc.end());
  return {std::move(e2), begin};
}

template<typename TokenIt>
std::pair<expression::ptr, TokenIt> parse_e_3(TokenIt begin, TokenIt end) {
#ifdef DEBUG
  std::cerr << "parse_e_3: " ;
    for(TokenIt t = begin; t!=end;++t)std::cerr <<" "<<t->to_string();
    std::cerr<<std::endl;
#endif
  auto[e4, nd] = parse_e_4(begin, end);
  begin = nd;
  while (begin != end) {
    try {
      auto[ne4, nd] = parse_e_4(begin, end);
      begin = nd;
      e4 = std::make_unique<fun_app>(std::move(e4), std::move(ne4), itr_sv(e4->loc.begin(), ne4->loc.end()));
    } catch (const std::runtime_error &) {
      break;
    }
  }
  return {std::move(e4), begin};
}

bool is_integer(std::string_view s) {
  return !s.empty() && (s.front() == '-' || ::isdigit(s.front())) && std::all_of(s.begin() + 1, s.end(), ::isdigit);
}

template<typename TokenIt>
std::pair<expression::ptr, TokenIt> parse_e_4(TokenIt begin, TokenIt end) {
#ifdef DEBUG
  std::cerr << "parse_e_4: " ;
    for(TokenIt t = begin; t!=end;++t)std::cerr <<" "<<t->to_string();
    std::cerr<<std::endl;
#endif
  if (begin == end) throw std::runtime_error("unexpected EOF");
  switch (begin->type) {
    case TRUE:return {std::make_unique<bool_literal>(true, begin->sv), ++begin};
    case FALSE:return {std::make_unique<bool_literal>(false, begin->sv), ++begin};
    case FREE: {
      if (int64_t n;std::from_chars(begin->sv.begin(), begin->sv.end(), n).ec == std::errc())return {std::make_unique<int_literal>(n, begin->sv), ++begin};
      //TODO: floating point literal, string literal
      return {std::make_unique<identifier>(begin->sv), ++begin};
    }
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
    case LET:throw std::runtime_error("unimplemented");
    case MATCH:throw std::runtime_error("unimplemented");
    default:return parse_e_1(begin, end);
  }
}

}

#endif //COMPILERS_BML_LIB_PARSE_H_
