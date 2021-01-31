#include <types/types.h>
#include <util/util.h>

namespace type {

namespace function {

tf_tuple_t &tf_tuple(size_t n) {
  static std::unordered_map<size_t, tf_tuple_t> tuples;
  return tuples.try_emplace(n, tf_tuple_t(n)).first->second;
}
void variant::print_with_args(const std::vector<expression::t> &args, std::ostream &os) const {
  assert(args.size() == n_args);
  if (args.empty()) {
    os << name;
  } else if (args.size() == 1) {
    args.front().print(os);
    os << " " << name;
  } else {
    os << "(";
    bool comma = false;
    for (const auto &p : args) {
      if (comma)os << ", ";
      comma = true;
      p.print(os);
    }
    os << ") " << name;
  }
}
void primitive::print_with_args(const std::vector<expression::t> &args, std::ostream &os) const {
  assert(args.empty());
  assert(n_args == 0);
  os << name;
}
void tf_fun_t::print_with_args(const std::vector<expression::t> &args, std::ostream &os) const {
  assert(args.size() == 2);
  if(std::holds_alternative<expression::application>(args[0])){
    const auto& ap = std::get<expression::application>(args[0]);
    const bool sq(dynamic_cast<const tf_fun_t*>(ap.f));
    if(sq)os<<"(";
    ap.print(os);
    if(sq)os<<")";
  }
  else args[0].print(os);
  os << " -> ";
  args[1].print(os);

}
void tf_tuple_t::print_with_args(const std::vector<expression::t> &args, std::ostream &os) const {
  assert(args.size() >= 2);
  bool comma = false;
  for (const auto &p : args) {
    if (comma)os << " * ";
    comma = true;
    if(std::holds_alternative<expression::application>(p)){
      const auto& ap = std::get<expression::application>(p);
      const bool sq(dynamic_cast<const tf_fun_t*>(ap.f) || dynamic_cast<const tf_tuple_t*>(ap.f));
      if(sq)os<<"(";
      ap.print(os);
      if(sq)os<<")";
    } else p.print(os);
  }

}
}

namespace expression {
t::t(const function::t *f, std::vector<t> &&args) : base(std::in_place_type_t<application>(), f, std::move(args)) {}

void t::print(std::ostream &os) const {
  std::visit(util::overloaded{
      [&os](variable v) { v.print(os); },
      [&os](const application &a) { a.print(os); }
  }, *static_cast<const base *>(this));
}
void variable::print(std::ostream &os) const {
  if (id < 26)os << '\'' << char('a' + id);
  else os << "'_" << id;
}
void application::print(std::ostream &os) const {
  return f->print_with_args(args, os);
}
bool t::check_poly_normalized(size_t &next_fresh) const {
  return std::visit(util::overloaded{
      [&next_fresh](variable v) {
        if (v.id > next_fresh)return false;
        if (v.id == next_fresh)++next_fresh;
        return true;
      },
      [&next_fresh](const application &a) {
        for (const t &x : a.args)if (!x.check_poly_normalized(next_fresh))return false;
        return true;
      }
  }, *static_cast<const base *>(this));
}
bool t::is_poly_normalized() const {
  size_t f = 0;
  return check_poly_normalized(f);
}
void t::poly_normalize() {
  std::unordered_map<size_t, size_t> relabel;
  execute_poly_normalize(relabel);
}
void t::execute_poly_normalize(std::unordered_map<size_t, size_t> &relabel) const {
  std::visit(util::overloaded{
      [&relabel](variable v) {
        v.id = relabel.try_emplace(v.id, relabel.size()).first->second;
      },
      [&relabel](const application &a) {
        for (const t &x : a.args)x.execute_poly_normalize(relabel);
      }
  }, *static_cast<const base *>(this));
}
}

type_map make_default_type_map() {
  type_map m;
  using namespace function;
  for(const primitive& p : {tf_int,tf_bool,tf_file,tf_string,tf_time,tf_unit}){
    m.try_emplace(p.name,&p);
  }
  return m;
}
}

std::ostream &operator<<(std::ostream &os, const type::expression::t &t) {
  t.print(os);
  return os;
}