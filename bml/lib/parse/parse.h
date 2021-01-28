#ifndef COMPILERS_BML_LIB_PARSE_H_
#define COMPILERS_BML_LIB_PARSE_H_

#include <ast/ast.h>
#include <array>
#include <cassert>
#include <algorithm>
#include <charconv>
#include <iostream>
#include "util/util.h"
#include <util/message.h>

namespace parse {
enum token_type {
  LITERAL, IDENTIFIER, CAP_NAME, PARENS_OPEN, PARENS_CLOSE,
  EQUAL, PIPE, ARROW, PLUS, MINUS, EOC, LET, REC, IN, AND, WITH, MATCH, FUN,
  COMMA, COLON, SEMICOLON, DOT, IF, THEN, ELSE, TRUE, FALSE, UNDERSCORE,
  LESS_THAN, GREATER_THAN, LESS_EQUAL_THAN, GREATER_EQUAL_THAN, STAR, SLASH,
  TYPE, NONREC, OF, NOT_EQUAL, DESTROY_ARROW, PIPE_LEFT, PIPE_RIGHT, END_OF_INPUT
};

namespace error {
class t : public std::runtime_error {
public:
  t() : std::runtime_error("parsing error") {}
};

class report_token : public t, public util::error::report_token_error {
public:
  report_token(std::string_view found, std::string_view before, std::string_view after)
      : util::error::report_token_error(before, found, after) {}

};

class unexpected_token : public t, public util::error::report_token_error {
public:
  unexpected_token(std::string_view found) : util::error::report_token_error("Token", found, "was not expected here") {}

};

class expected_token_found_another : public t, public util::error::report_token_error {
public:
  expected_token_found_another(std::string_view expected, std::string_view found)
      : util::error::report_token_error(std::string("Expected ").append(expected).append(" but found"), found, "") {}
};
}

namespace {
typedef std::pair<std::string_view, token_type> st;
}
constexpr auto tokens_map = util::make_array(
    st{"(", PARENS_OPEN},
    st{")", PARENS_CLOSE},
    st{"<|", PIPE_LEFT}, st{"|>", PIPE_RIGHT},
    st{"=", EQUAL}, st{"|", PIPE},
    st{"->", ARROW}, st{"-", MINUS},
    st{"+", PLUS}, st{";;", EOC},
    st{"let", LET}, st{"rec", REC},
    st{"fun", FUN},
    st{"nonrec", NONREC}, st{"of", OF},
    st{"in", IN}, st{"and", AND},
    st{"with", WITH}, st{"match", MATCH},
    st{",", COMMA}, st{":", COLON},
    st{";", SEMICOLON}, st{".", DOT},
    st{"if", IF}, st{"then", THEN}, st{"else", ELSE},
    st{"true", TRUE}, st{"false", FALSE}, st{"_", UNDERSCORE},
    st{"*", STAR}, st{"/", SLASH}, st{"type", TYPE},
    st{"<=", LESS_EQUAL_THAN}, st{">=", GREATER_EQUAL_THAN},
    st{"<>", NOT_EQUAL},
    st{"<", LESS_THAN}, st{">", GREATER_THAN},
    st{"~>", DESTROY_ARROW});

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
      case FUN:return "FUN";
      case UNDERSCORE:return "UNDERSCORE";
      case LESS_THAN:return "LESS_THAN";
      case GREATER_THAN:return "GREATER_THAN";
      case LESS_EQUAL_THAN:return "LESS_EQUAL_THAN";
      case GREATER_EQUAL_THAN:return "GREATER_EQUAL_THAN";
      case STAR:return "STAR";
      case SLASH:return "SLASH";
      case TYPE:return "TYPE";
      case NONREC:return "NONREC";
      case OF:return "OF";
      case NOT_EQUAL:return "NOT_EQUAL";
      case DESTROY_ARROW:return "DESTROY_ARROW";
      case PIPE_LEFT:return "PIPE_LEFT";
      case PIPE_RIGHT:return "PIPE_RIGHT";
      case END_OF_INPUT:return "END_OF_INPUT";
    }
    return std::string("unimplemented (").append(sv).append(")");
    //assert(false);
  }
};

class tokenizer {
public:
  tokenizer(const tokenizer &) = default;
  tokenizer(tokenizer &&) = default;
  tokenizer &operator=(const tokenizer &) = default;
  tokenizer &operator=(tokenizer &&) = default;
  explicit tokenizer(std::string_view source);
  token_type peek() const;
  token peek_full() const;
  std::string_view peek_sv() const;
  token pop();
  bool empty() const;
  void expect_pop(token_type);
  void expect_peek(token_type);
  void expect_peek_any_of(std::initializer_list<token_type>);
  void unexpected_token();
  void print_errors();
private:
  void write_head();
  std::string_view to_parse, source;
  token head;
};

}

namespace ast {

using namespace parse;

namespace expression {
ptr parse(tokenizer &tk);
}

namespace matcher {
ptr parse(tokenizer &tk);

}

namespace definition {
ptr parse(tokenizer &tk);

}

namespace literal {
ptr parse(const token &t);
}

namespace type {
namespace expression {
ptr parse(tokenizer &tk);
}
namespace definition {
ptr parse(tokenizer &tk);
}
}

}

#endif //COMPILERS_BML_LIB_PARSE_H_
