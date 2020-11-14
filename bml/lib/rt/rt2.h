#ifndef COMPILERS_BML_LIB_RT_RT_H_
#define COMPILERS_BML_LIB_RT_RT_H_
#include <cstdint>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <new>

namespace rt {

static constexpr int64_t INT63_MAX = 0x3fffffffffffffff; // 2^62 -1
static constexpr int64_t INT63_MIN = 0xc000000000000000; // - 2^62
static constexpr uint64_t UINT63_MAX = 0x7fffffffffffffff; // 2^63 -1
static constexpr uint64_t UINT63_MIN = 0x0000000000000000; // - 2^63

struct value;
struct block;

struct value {

  uintptr_t v;
  explicit value() : v(0) {}
  [[nodiscard]] constexpr bool is_immediate() const {
    return v & 1;
  }
  [[nodiscard]] constexpr int64_t to_int() const {
    return int64_t(v) >> 1;
  }
  [[nodiscard]] static value from_int(int64_t x) {
    return value((x << 1) | 1);
  }

  [[nodiscard]] static value from_raw(uintptr_t x) {
    return value(x);
  }

  [[nodiscard]] constexpr uint64_t to_uint() const {
    return uint64_t(v) >> 1;
  }
  [[nodiscard]] static value from_uint(uint64_t x) {
    return value((x << 1) | 1);
  }

  [[nodiscard]] constexpr bool is_block() const {
    return !is_immediate();
  }
  [[nodiscard]] const block *to_block() const {
    return reinterpret_cast<const block *>(v);
  }
  [[nodiscard]] static value from_block(const block *ptr) {
    return value(uintptr_t(ptr));
  }
  bool operator==(const value& o) const {return v == o.v;};
 private:
  constexpr explicit value(uintptr_t v) : v(v) {}
};

static_assert(sizeof(value) == 8, "rt::value should be a 64-bit word");

struct block {
  enum tag_t : uint64_t { FN_BASE_PURE = 0b01, FN_BASE_CLOSURE = 0b11, FN_ARG = 0b101, ARRAY = 0b10 };
  tag_t tag;
  constexpr block(tag_t tag) : tag(tag) {}
};

struct fn_node : public block {
  size_t n_args_left;
  constexpr fn_node(tag_t t, size_t n_args_left) : block(t), n_args_left(n_args_left) {}
};

struct fn_base : public fn_node {
  using fn_node::fn_node;
};

struct fn_arg : public fn_node {
  const fn_node *prev;
  value arg;
  fn_arg(const fn_node *p, value x) : fn_node(FN_ARG, p->n_args_left - 1), arg(x), prev(p) {}
};

struct fn_base_pure : public fn_base {
  typedef value (*text_ptr_t)(value);
  text_ptr_t text_ptr;
  constexpr fn_base_pure(size_t n_args, text_ptr_t p) : fn_base(FN_BASE_PURE, n_args), text_ptr(p) {}
};

struct fn_base_closure : public fn_base {
  typedef value (*text_ptr_t)(value, value);//args,closure block
  text_ptr_t text_ptr;
  //TODO: store closure
  fn_base_closure(size_t n_args, text_ptr_t p) : fn_base(FN_BASE_CLOSURE, n_args), text_ptr(p) {}
};

value apply_fn(value f, value x) {
  //assert(f.is_block());
  //assert(f.to_block()->tag == block::FN_ARG || f.to_block()->tag == block::FN_BASE_CLOSURE || f.to_block()->tag == block::FN_BASE_PURE); // is fn_node
  const fn_node *nf = reinterpret_cast<const fn_node *> (f.to_block());
  const fn_node *n = new(malloc(sizeof(fn_arg))) fn_arg(nf, x);
  if (n->n_args_left)return value::from_block(n);
  //assert(n->n_args_left);


  while (nf->tag == block::FN_ARG)nf = reinterpret_cast<const fn_arg *>(nf)->prev;
  //assert(s == 0);
  if (nf->tag == block::FN_BASE_PURE) {
    return reinterpret_cast<const fn_base_pure *>(nf)->text_ptr(value::from_block(n));
  } else {
    //assert(root->tag == block::FN_BASE_CLOSURE);
    return reinterpret_cast<const fn_base_closure *>(nf)->text_ptr(value::from_block(n), value::from_block(nf));
  }

}


value int_sum_fn(value v) {
  //(v.is_block());
  const fn_arg* x = reinterpret_cast<const fn_arg *>(v.to_block());
  //assert(a.size==2);
  return value::from_int(x->arg.to_int() +  reinterpret_cast<const fn_arg*>(x->prev)->arg.to_int());
}
fn_base_pure int_sum(2,int_sum_fn);
value int_print_fn(value v) {
  const auto* x = reinterpret_cast<const fn_arg *>(v.to_block());
  printf("%ld",x->arg.to_int());
  return value();
}
fn_base_pure int_print(1,int_print_fn);

}

#endif //COMPILERS_BML_LIB_RT_RT_H_
