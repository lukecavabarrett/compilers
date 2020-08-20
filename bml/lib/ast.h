#ifndef COMPILERS_BML_LIB_AST_H_
#define COMPILERS_BML_LIB_AST_H_
#include <memory>
#include <vector>
#include <variant>

namespace ast {

namespace {
std::string char_to_html(char c) {
  if (c == ' ')return "&nbsp;";
  if (c == '\n')return "<br>";
  return std::string{c};
}
}

struct code_piece {
  std::string_view loc;
  code_piece(std::string_view loc) : loc(loc) {}
  virtual std::string _make_html_label() const = 0;
  virtual void _make_html_childcall(std::string &out, std::string_view::iterator &it) const = 0;
  void make_html(std::string &out, std::string_view::iterator &it) const {
    while (it < loc.begin()) {
      out.append(char_to_html(*it));
      ++it;
    }
    out.append(R"(<span class="box" title=")").append(_make_html_label()).append("\">");
    _make_html_childcall(out, it);
    while (it < loc.end()) {
      out.append(char_to_html(*it));
      ++it;
    }
    out.append("</span>");
  };
  std::string to_html() const {
    std::string out = "<html>\n"
                      "<head>\n"
                      "    <style type=\"text/css\">\n"
                      "        .box {\n"
                      "            font-size: 20px;\n"
                      "            font-family: monospace;\n"
                      "            /*outline: 1px dotted lightgray;*/\n"
                      "        }\n"
                      "\n"
                      "        .hover {\n"
                      "            outline: 1px solid black;\n"
                      "            background-color: #ffff00;\n"
                      "        }\n"
                      "    </style>\n"
                      "    <script src=\"https://ajax.googleapis.com/ajax/libs/jquery/3.5.1/jquery.min.js\"></script>\n"
                      "</head>\n"
                      "<body>\n"
                      "<script>\n"
                      "    $(function () {\n"
                      "        $('.box').on('mouseenter', function (e) {\n"
                      "            if (this == e.target) $(this).addClass('hover');\n"
                      "            $(this).parent().removeClass('hover');\n"
                      "        }).on('mouseleave', function (e) {\n"
                      "            $(this).removeClass('hover');\n"
                      "            if ($(this).parent().hasClass('box')) {\n"
                      "                $(this).parent().addClass('hover');\n"
                      "            }\n"
                      "        });\n"
                      "    })\n"
                      "</script>";
    auto it = loc.begin();
    make_html(out, it);
    out.append("</body>\n"
               "</html>");
    return out;
  }

};

struct expression : public code_piece {
  typedef std::unique_ptr<expression> ptr;
  using code_piece::code_piece;
};

struct literal : public expression {
  typedef std::unique_ptr<literal> ptr;
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {};
  using expression::expression;
};

struct matcher : public code_piece {
  typedef std::unique_ptr<matcher> ptr;
  using code_piece::code_piece;

  // UniversalIgnore | UniversalBind of string | Literal of Literal.t | Tuple of t list | Constructor of matcher

};

struct universal_matcher : public matcher {
  std::string _make_html_label() const final { return "Universal matcher"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {};

  typedef std::unique_ptr<universal_matcher> ptr;
  using matcher::matcher;
};

struct anonymous_universal_matcher : public universal_matcher {

  typedef std::unique_ptr<anonymous_universal_matcher> ptr;
  using universal_matcher::universal_matcher;

};

struct constructor_matcher : public matcher {
  std::string _make_html_label() const final { return "Constructor matcher"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    if(arg)arg->make_html(out, it);
  };
  typedef std::unique_ptr<constructor_matcher> ptr;
  matcher::ptr arg;
  std::string_view cons;
  constructor_matcher(matcher::ptr&& m,std::string_view loc,std::string_view c) : matcher(loc), arg(std::move(m)), cons(c) {}
  constructor_matcher(std::string_view loc) : matcher(loc), arg(), cons(loc) {}

};

struct literal_matcher : public matcher {
  std::string _make_html_label() const final { return "Literal matcher"; }

  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {};
  typedef std::unique_ptr<literal_matcher> ptr;
  literal::ptr value;
  literal_matcher(literal::ptr&& lit,std::string_view loc) : matcher(loc), value(std::move(lit)) {}
};

struct tuple_matcher : public matcher {
  std::string _make_html_label() const final { return "Match tuple"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    for (const auto &p : args)p->make_html(out, it);
  };
  std::vector<matcher::ptr> args;
  tuple_matcher(std::vector<matcher::ptr> &&args, std::string_view loc) : matcher(loc), args(std::move(args)) {}
  tuple_matcher(std::string_view loc) : matcher(loc) {}
};

struct single_definition : code_piece {
  typedef std::unique_ptr<single_definition> ptr;
  expression::ptr body;
  single_definition(expression::ptr&& b,std::string_view l) : code_piece(l), body(std::move(b)) {}
};

struct function_definition : single_definition {
  typedef std::unique_ptr<function_definition> ptr;
  std::string _make_html_label() const final { return "Function definition"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    name->make_html(out,it);
    for (const auto &p : args)p->make_html(out, it);
    body->make_html(out,it);
  };
  universal_matcher::ptr name;
  std::vector<matcher::ptr> args;
  using single_definition::single_definition;
  function_definition() : single_definition(expression::ptr(),std::string_view()) {}
};

struct value_definition : single_definition {
  typedef std::unique_ptr<value_definition> ptr;
  std::string _make_html_label() const final { return "Value binding"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    binded->make_html(out,it);
    body->make_html(out,it);
  };
  matcher::ptr binded;
  value_definition(matcher::ptr&& bi, expression::ptr&& bo, std::string_view loc) : single_definition(std::move(bo),loc), binded(std::move(bi)) {}
};

struct definition : code_piece {
  bool rec = false;
  std::vector<single_definition::ptr> defs;
  typedef std::unique_ptr<definition> ptr;
  std::string _make_html_label() const final { return "Definition(s)"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    for (const auto &p : defs)p->make_html(out, it);
  };
  definition () : code_piece(std::string_view()) {} ;
};


struct int_literal : public literal {
  std::string _make_html_label() const final { return "Int Literal"; }

  int64_t value;
  int_literal(int64_t value, std::string_view loc) : literal(loc), value(value) {}
};
struct bool_literal : public literal {
  std::string _make_html_label() const final { return "Bool Literal"; }

  bool value;
  bool_literal(bool value, std::string_view loc) : literal(loc), value(value) {}
};
struct unit_literal : public literal {
  using literal::literal;
  std::string _make_html_label() const final { return "Unit Literal"; }
};

struct identifier : public expression {
  std::string _make_html_label() const final { return "Identifier"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {};

  //const universal_bind &definition_point;
  using expression::expression;
};

struct if_then_else : public expression {
  std::string _make_html_label() const final { return "If_then_else"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    condition->make_html(out, it);
    true_branch->make_html(out, it);
    false_branch->make_html(out, it);
  };

  expression::ptr condition, true_branch, false_branch;
  if_then_else(expression::ptr &&condition, expression::ptr &&true_branch, expression::ptr &&false_branch, std::string_view loc)
      : expression(loc), condition(std::move(condition)), true_branch(std::move(true_branch)), false_branch(std::move(false_branch)) {}
};

struct build_tuple : public expression {
  std::string _make_html_label() const final { return "Build_tuple"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    for (const auto &p : args)p->make_html(out, it);
  };
  std::vector<expression::ptr> args;
  build_tuple(std::vector<expression::ptr> &&args, std::string_view loc) : expression(loc), args(std::move(args)) {}
  build_tuple(std::string_view loc) : expression(loc) {}
};

struct fun_app : public expression {
  std::string _make_html_label() const final { return "Function application"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    f->make_html(out, it);
    x->make_html(out, it);
  };
  expression::ptr f, x;
  fun_app(expression::ptr &&f, expression::ptr &&x, std::string_view loc) : expression(loc), f(std::move(f)), x(std::move(x)) {}

};

struct seq : public expression {
  std::string _make_html_label() const final { return "Sequence"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    a->make_html(out, it);
    b->make_html(out, it);
  };
  expression::ptr a, b;
  seq(expression::ptr &&a, expression::ptr &&b, std::string_view loc) : expression(loc), a(std::move(a)), b(std::move(b)) {}

};

struct match_with : public expression {
  std::string _make_html_label() const final { return "Match_with"; }
  void _make_html_childcall(std::string &out, std::string_view::iterator &it) const final {
    what->make_html(out,it);
    for (const auto &[p,r] : branches){p->make_html(out, it);r->make_html(out,it);}
  };
  typedef std::unique_ptr<match_with> ptr;
  struct branch {
    matcher::ptr pattern;
    expression::ptr result;
  };
  expression::ptr what;
  std::vector<branch> branches;
  match_with(expression::ptr&& w,std::string_view loc) : expression(loc), what(std::move(w)) {}

};

struct let_in : public expression {

};

}

#endif //COMPILERS_BML_LIB_AST_H_
