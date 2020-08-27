#ifndef COMPILERS_BML_LIB_PARSE_H_
#define COMPILERS_BML_LIB_PARSE_H_

#include <ast.h>
#include <array>
#include <cassert>
#include <algorithm>
#include <charconv>
#include <iostream>
#include "util.h"
#include <errmsg.h>

namespace parse {
enum token_type { LITERAL, IDENTIFIER, CAP_NAME, PARENS_OPEN, PARENS_CLOSE, EQUAL, PIPE, ARROW, PLUS, MINUS, EOC, LET, REC, IN, AND, WITH, MATCH, COMMA, COLON, SEMICOLON, DOT, IF, THEN, ELSE, TRUE, FALSE, UNDERSCORE, END_OF_INPUT };

class error : std::runtime_error {
  using std::runtime_error::runtime_error;
};

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



class tokenizer {
 public:
  explicit tokenizer(std::string_view source);
  token_type peek() const;
  token peek_full() const;
  std::string_view peek_sv() const;
  token pop();
  bool empty() const;
  void expect_pop(token_type);
  void expect_peek(token_type);
  void unexpected_token();
  void print_errors();
 private:
  void write_head();
  std::string_view to_parse;
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


}

#endif //COMPILERS_BML_LIB_PARSE_H_
