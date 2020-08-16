#include <util.h>


std::string_view itr_sv(std::string_view::iterator begin,std::string_view::iterator end){
  return std::string_view(begin,end-begin);
}