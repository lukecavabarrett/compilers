#include <util.h>


std::string_view itr_sv(std::string_view::iterator begin,std::string_view::iterator end){
  return std::string_view(begin,end-begin);
}

std::string load_file(std::string_view path){
  std::ifstream t(std::string(path).c_str());
  return std::string((std::istreambuf_iterator<char>(t)),
                     std::istreambuf_iterator<char>());
}