#include <parse.h>
#include <unordered_map>
#include <algorithm>
#include <cassert>
#include <util.h>

bool allowed_in_identifier(char c){
  return ::isalnum(c) || c=='_' ;
}

bool startswith_legal(std::string_view a,std::string_view b){
  if(b.size()>a.size())return false;
  if(!std::equal(b.begin(),b.end(),a.begin()))return false;
  if(allowed_in_identifier(b.back()) && a.size()>b.size() && allowed_in_identifier(a[b.size()]))return false;
  return true;
}




std::vector<token> tokenize(std::string_view s) {
  std::vector<token> tks;

#define trim_prefix while(!s.empty() && s.front()<=32)s.remove_prefix(1);
  trim_prefix
  while(!s.empty()) {
    bool mtc = false;
    for(const auto& [p,t] : tokens_map)if(startswith_legal(s,p)) {
        tks.push_back({.sv=std::string_view(s.begin(),p.size()),.type=t});
        s.remove_prefix(p.size());
        mtc = true;
        trim_prefix
        break;
    }
    if(mtc)continue;
    auto end = std::find_if_not(s.begin(),s.end(),allowed_in_identifier);
    if(s.begin()==end){
      throw std::runtime_error(formatter() << "cannot parse anymore: " << int(s.front()) << s >> formatter::to_str);
    }
    tks.push_back({.sv=itr_sv(s.begin(),end),.type=token_type::FREE});
    s.remove_prefix(end-s.begin());
    trim_prefix
  }
  return tks;
#undef trim_prefix
}
