#include <util.h>

namespace util {
std::string_view itr_sv(std::string_view::iterator begin, std::string_view::iterator end) {
  return std::string_view(begin, end - begin);
}

std::string load_file(std::string_view path) {
  std::ifstream t(std::string(path).c_str());
  return std::string((std::istreambuf_iterator<char>(t)),
                     std::istreambuf_iterator<char>());
}
namespace chars {
bool is_escaped_in_string_literal(char c) {
  if (c < 32)return true;
  if (c == '"')return true;
  if (c == '\\')return true;
  return false;
}

bool has_escaped_mnemonic(char c) {
  switch (c) {
    case '\'':return true;
    case '\"':return true;
    case '\\':return true;
    case '\n':return true;
    case '\r':return true;
    case '\t':return true;
    case '\b':return true;
    case '\f':return true;
    case '\a':return true;
    case '\v':return true;
    default: return false;
  }
}

char escaped_mnemonic(char c) {
  switch (c) {
    case '\'':return c;
    case '\"':return c;
    case '\\':return c;
    case '\n':return 'n';
    case '\r':return 'r';
    case '\t':return 't';
    case '\b':return 'b';
    case '\f':return 'f';
    case '\a':return 'a';
    case '\v':return 'a';
    default: THROW_INTERNAL_ERROR;
  }
}

bool is_valid_mnemonic(char c){
  switch (c) {
    case '\'':
    case '\"':
    case '\\':
    case 'n':
    case 'r':
    case 't':
    case 'b':
    case 'f':
    case 'a':
    case 'v':return true;
    default: return false;
  }
}
char parse_mnemonic(char c){
  switch (c) {
    case '\'':return c;
    case '\"':return c;
    case '\\':return c;
    case 'n':return '\n';
    case 'r':return '\r';
    case 't':return '\t';
    case 'b':return '\b';
    case 'f':return '\f';
    case 'a':return '\a';
    case 'v':return '\a';
    default: THROW_INTERNAL_ERROR;
  }
}

}
}

