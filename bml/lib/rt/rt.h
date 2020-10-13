#ifndef COMPILERS_BML_LIB_RT_RT_H_
#define COMPILERS_BML_LIB_RT_RT_H_
#include <cstdint>
#include <cstdlib>
#include <cassert>
#include <cstring>

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
 private:
  [[nodiscard]] constexpr explicit value(uintptr_t v) : v(v) {}
};

static_assert(sizeof(value) == 8, "rt::value should be a 64-bit word");

struct block {
  enum tag_t : uint64_t { FN_BASE_PURE = 0b01, FN_BASE_CLOSURE = 0b11, FN_ARG = 0b101, ARRAY = 0b10 };
  tag_t tag;
  constexpr block(tag_t tag) : tag(tag) {}
};

struct array : public block {
  size_t size;
  static array *make(size_t size) {
    static_assert(sizeof(array) == 16);
    static_assert(alignof(array) == 8);
    void *mem = malloc(16 + size * 8);
    std::memset(mem, 0, 16 + size * 8);
    array *p = reinterpret_cast<array *>(mem);
    p->tag = block::ARRAY;
    p->size = size;
    return p;
  }
  value &operator[](size_t idx) {
    //assert(idx < size);
    return reinterpret_cast<value *>(this)[idx + 2];
  }
  value &at(size_t idx) {
    //assert(idx < size);
    return reinterpret_cast<value *>(this)[idx + 2];
  }
  const value &operator[](size_t idx) const {
    //assert(idx < size);
    return reinterpret_cast<const value *>(this)[idx + 2];
  }
  const value &at(size_t idx) const {
    //assert(idx < size);
    return reinterpret_cast<const value *>(this)[idx + 2];
  }
 private:

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
  constexpr fn_base_pure(size_t n_args, text_ptr_t p) : fn_base(FN_BASE_PURE,n_args), text_ptr(p) {}
};

struct fn_base_closure : public fn_base {
  typedef value (*text_ptr_t)(value, value);//args,closure block
  text_ptr_t text_ptr;
  //TODO: store closure
  fn_base_closure(size_t n_args, text_ptr_t p)  : fn_base(FN_BASE_CLOSURE,n_args), text_ptr(p) { }
};

value apply_fn(value f, value x) {
  //assert(f.is_block());
  //assert(f.to_block()->tag == block::FN_ARG || f.to_block()->tag == block::FN_BASE_CLOSURE || f.to_block()->tag == block::FN_BASE_PURE); // is fn_node
  const fn_node *n = reinterpret_cast<const fn_node *> (f.to_block());
  //assert(n->n_args_left);
  if (n->n_args_left == 1) {
    const fn_node *root = n;
    while (root->tag == block::FN_ARG)root = reinterpret_cast<const fn_arg *>(root)->prev;
    array *args_array = array::make(root->n_args_left);
    size_t s = root->n_args_left - 1;
    args_array->at(s) = x;
    for (const fn_node *p = n; p->tag == block::FN_ARG; p = reinterpret_cast<const fn_arg *>(p)->prev) {
      --s;
      args_array->at(s) = reinterpret_cast<const fn_arg *>(p)->arg;
    }
    //assert(s == 0);
    if (root->tag == block::FN_BASE_PURE) {
      return reinterpret_cast<const fn_base_pure *>(root)->text_ptr(value::from_block(args_array));
    } else {
      //assert(root->tag == block::FN_BASE_CLOSURE);
      return reinterpret_cast<const fn_base_closure *>(root)->text_ptr(value::from_block(args_array), value::from_block(root));
    }
  } else {
    return value::from_block(new fn_arg(n, x));
  }

}

}

#endif //COMPILERS_BML_LIB_RT_RT_H_
