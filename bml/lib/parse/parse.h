#ifndef COMPILERS_BML_LIB_PARSE_H_
#define COMPILERS_BML_LIB_PARSE_H_

#include <ast/ast.h>
#include <array>
#include <cassert>
#include <algorithm>
#include <charconv>
#include <iostream>
#include <util/util.h>
#include <util/message.h>

namespace parse {
enum token_type {
  LITERAL, IDENTIFIER, CAP_NAME, PARENS_OPEN, PARENS_CLOSE, PHYS_EQUAL, NOT_PHYS_EQUAL,
  EQUAL, PIPE, ARROW, PLUS, MINUS, EOC, LET, REC, IN, AND, WITH, MATCH, FUN,
  COMMA, COLON, SEMICOLON, DOT, IF, THEN, ELSE, TRUE, FALSE, UNDERSCORE,
  LESS_THAN, GREATER_THAN, LESS_EQUAL_THAN, GREATER_EQUAL_THAN, STAR, SLASH,
  TYPE, NONREC, OF, NOT_EQUAL, DESTROY_ARROW, PIPE_LEFT, PIPE_RIGHT, END_OF_INPUT
};

namespace error {
class t : public std::runtime_error {
public:
  t() : std::runtime_error("ast::parse::error") {}
};

template<typename Style>
struct style_token : public t, public util::message::report_token_front_back<Style> {
  style_token(std::string_view front, std::string_view token, std::string_view back,
              std::string_view file, std::string_view filename) : util::message::report_token_front_back<Style>(front,
                                                                                                                token,
                                                                                                                back,
                                                                                                                file,
                                                                                                                filename) {}
  style_token(std::string_view front, std::string_view token, std::string_view back)
      : util::message::report_token_front_back<Style>(front,
                                                      token,
                                                      back,
                                                      "",
                                                      "") {}
};

typedef style_token<util::message::style::error> report_token;
typedef style_token<util::message::style::note> note_token;

struct unexpected_token : public t, public util::message::error_token {
  std::string_view additional_note;
  unexpected_token(std::string_view found, std::string_view additional_note = "") : util::message::error_token(found),additional_note(additional_note) {}
  void describe(std::ostream &os) const {
    os << "token " << util::message::style::bold << token << util::message::style::clear << " was not expected here.";
    if(!additional_note.empty())os<<additional_note;
  }
};

struct expected_token_found_another : public t, public util::message::error_token {
public:
  std::string_view expected;
  expected_token_found_another(std::string_view expected,
                               std::string_view found)
      : util::message::error_token(found, "", ""), expected(expected) {}
  void describe(std::ostream &os) const {
    os << "token " << util::message::style::bold << expected << util::message::style::clear << " was expected, but "
       << util::message::style::bold << token << util::message::style::clear << " was found";
  }
};

struct multi : public t, public util::message::vector {
  typedef util::message::vector vec_t;
  using vec_t::vec_t;
};

}

namespace {
typedef std::pair<std::string_view, token_type> st;
}
constexpr auto tokens_map = util::make_array(
    st{"(", PARENS_OPEN}, st{")", PARENS_CLOSE},
    st{"<|", PIPE_LEFT}, st{"|>", PIPE_RIGHT},
    st{"==", PHYS_EQUAL}, st{"!==", NOT_PHYS_EQUAL},
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
  explicit tokenizer(std::string_view source, std::string_view filename = "source.ml");
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
  std::string_view get_source() const;
  std::string_view get_filename() const;
private:
  void write_head();
  std::string_view to_parse, source, filename;
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
