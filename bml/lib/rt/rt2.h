#ifndef COMPILERS_BML_LIB_RT_RT_H_
#define COMPILERS_BML_LIB_RT_RT_H_
#include <cstdint>
#include <cstdio>
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
  [[nodiscard]] value* to_block()  {
    return reinterpret_cast<value*>(v);
  }
  bool operator==(const value& o) const {return v == o.v;};
 private:
  constexpr explicit value(uintptr_t v) : v(v) {}
};

static_assert(sizeof(value) == 8, "rt::value should be a 64-bit word");



}



#endif //COMPILERS_BML_LIB_RT_RT_H_
