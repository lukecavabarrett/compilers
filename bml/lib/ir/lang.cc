#include <lang.h>

namespace ir::lang {
memory_access var::operator*() const {
  return memory_access{.base = *this, .block_offset = 0};
}
memory_access var::operator[](size_t idx) const {
  return memory_access{.base = *this, .block_offset = idx};
}

INSTR_OPS_2(mov)
INSTR_OPS_2(add)
INSTR_OPS_2(cmp)
INSTR_OPS_1(sar)
INSTR_OPS_1(ret)

void scope::push_back(instruction &&i) {
  ops.emplace_back(std::move(i));
}
void scope::push_back(ternary &&t) {
  ops.emplace_back(std::move(t));
}
scope &scope::operator<<(instruction &&i) {
  push_back(std::move(i));
  return *this;
}
scope &scope::operator<<(ternary &&i) {
  push_back(std::move(i));
  return *this;
}
}
