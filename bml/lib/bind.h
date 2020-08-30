#ifndef COMPILERS_BML_LIB_BIND_H_
#define COMPILERS_BML_LIB_BIND_H_

#include <cstddef>
#include <map>
#include <vector>
#include <memory>
#include <util/util.h>
#include <util/message.h>
#include <iostream>
namespace bind {

namespace error {
class t : public std::runtime_error {
 public:
  t() : std::runtime_error("binding error"){}
};

class unbound_value : public t, public util::error::report_token_error {
 public:
  unbound_value(std::string_view identifier) : util::error::report_token_error("Unbound value",identifier,"") {}
};

//TODO: error unbound value with suggestion
}



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
   public:
    void bind(std::string_view s,ref r){
      if(map.find(std::string(s))!=map.end()){
        throw std::runtime_error("multiple bind");
      }
      map.try_emplace(std::string(s),r);
    }

    map_node(const map_node&) = default;
    map_node(map_node&&) = default;
    map_node() = default;
    //TODO: fix encapsulation for make_shared
    map_node(const std::shared_ptr<map_node>& p) : parent(p) {}

    static std::shared_ptr<map_node> __make_shared(const std::shared_ptr<map_node>& p) {
      struct make_shared_enabler : public map_node {
        using map_node::map_node;
      };

      return std::make_shared<make_shared_enabler>(p);
    }
  };
  typedef map_node map_t;
 private:
  typedef std::shared_ptr<map_node> ptr;

 public:
  name_table sub_table() const {

    return name_table(map_node::__make_shared(head));
  }
  map_node& map() {return *head.get();}
  ref lookup(std::string_view s) const {
    for(ptr p = head;p;p = p.get()->parent)
      if(auto it = p->map.find(std::string(s));it!=p->map.end())
        return it->second;
    throw error::unbound_value(s);
   }
  name_table() : head(nullptr) {}
  name_table(const name_table&) = default;
  name_table(name_table&&) = default;
  name_table& operator=(const name_table&) = default;
  name_table& operator=(name_table&&) = default;

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
