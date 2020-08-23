#include <ast.h>

namespace ast {
 code_piece::code_piece(std::string_view loc) : loc(loc) {}
void code_piece::make_html(std::string &out, std::string_view::iterator &it) const {
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
std::string code_piece::to_html() const {
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





}
