#include <ast.h>
#include <util/sexp.h>
#include <parse.h>

namespace ast {
void locable::make_html(std::string &out, std::string_view::iterator &it) const {
  while (it < loc.begin()) {
    out.append(char_to_html(*it));
    ++it;
  }
  out.append(R"(<span class="box" title=")").append(html_description()).append("\">");
  _make_html_childcall(out, it);
  while (it < loc.end()) {
    out.append(char_to_html(*it));
    ++it;
  }
  out.append("</span>");
}
std::string locable::to_html() const {
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

namespace expression {

free_vars_t identifier::free_vars() {
  if (definition_point && definition_point->top_level) return {};
  return {{name, {this}}};
}

capture_set identifier::capture_group() {
  assert(definition_point); //TODO: internal fail
  if (definition_point->top_level) return {};
  return {definition_point};
}
void identifier::compile(sections_t s, size_t stack_pos) {
  if (definition_point->top_level) {
    definition_point->globally_evaluate(s.main);
  } else if (s.def_fun && s.def_fun->is_capturing(definition_point)) {
    THROW_UNIMPLEMENTED
  } else {

    s.main << "mov rax, qword [rsp";
    if (stack_pos > definition_point->stack_relative_pos) {
      s.main << "+" << 8 * (stack_pos - definition_point->stack_relative_pos);
    }
    s.main << "]  ; retrieving " << name << " from local scope\n";
  }
}

}

namespace definition {
}




/*
TODO: clean this for being binded

 std::optional<
    std::pair<
        std::vector<
            std::variant<definition::ptr,
                         expression::ptr>
        >,
        ltable>
> compile(std::string_view source, const ltable &__lt,std::string_view filename) {
  tokenizer tk(source);
  std::vector<std::variant<definition::ptr, expression::ptr> > asts;
  ltable lt = __lt;
  try {
    while (!tk.empty()) {
      bool was_def = false;
      if (tk.peek() == LET) {
        tokenizer tk_copy = tk;
        definition::ptr d = definition::parse(tk);
        if (tk.peek() == IN) {
          was_def = false;
          tk = tk_copy;
        } else {
          was_def = true;
          tk.expect_pop(EOC);
          lt = d->bind(lt);
        }
      }
      if (!was_def) {
        expression::ptr e = expression::parse(tk);
        tk.expect_pop(EOC);
        e->bind(lt);
      }

    }
  } catch (const util::error::message& e) {
    e.print(std::cout,source,filename);
    return {};
  }
  return std::make_pair(std::move(asts),lt);

}
*/
}
