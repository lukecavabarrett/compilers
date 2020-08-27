#ifndef COMPILERS_BML_LIB_BIND_H_
#define COMPILERS_BML_LIB_BIND_H_

#include <cstddef>
#include <map>
#include <vector>
#include <memory>

namespace bind {

struct error {}; //TODO: enrich

/*
 In general map gets forwarded as they are in most expressions, copied in all places.
 Two cases of introduction:
 let DEFINITIONS in EXPRESSION -> Definitions is going to add something for the expression. so a definition should take a name_table and return an enriched one.
 let [rec] m1 m2 m3 = e -> e would like to define
 */
template<typename T>
class name_table {
 public:
typedef T& ref;
 private:
 public:
  class map_node {
   friend class name_table;
    std::map<std::string,ref,std::less<>> map;
    std::shared_ptr<map_node> parent;
    map_node(const std::shared_ptr<map_node>& p) : parent(p) {}
  public:
    void bind(std::string_view s,ref r){
      if(map.find(std::string(s))!=map.end()){
        throw std::runtime_error("multiple bind");
      }
      map.try_emplace(std::string(s),r);
    }
  };
  typedef map_node map_t;
 private:
  typedef std::shared_ptr<map_node> ptr;
 public:
  name_table sub_table() const {
    return name_table(std::make_shared<map_node>(head));
  }
  map_node& map() {return *head.get();}
  std::optional<ref> lookup(std::string_view s) const {
    for(const ptr& p = head;p;p = p.get()->parent)
      if(auto it = p->map.find(std::string(s));it!=p->map.end())
        return it->ref;
    return {};
   }
  name_table() : head(nullptr) {}
  name_table(const name_table&) = default;
  name_table(name_table&&) = default;

 private:
  ptr head;
  name_table(const ptr& p) : head(p) {}
};

}

namespace ast {

namespace expression {

}

}

#endif //COMPILERS_BML_LIB_BIND_H_
