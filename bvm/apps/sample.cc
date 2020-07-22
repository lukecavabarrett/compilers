#include <bvm.h>
#include <iostream>
#include <cassert>

void test_literal_parse(){
  assert(bvm::operand::immediate::parse("$010")->value == 8);
  assert(bvm::operand::immediate::parse("$10")->value == 10);
  assert(bvm::operand::immediate::parse("$0x10")->value == 16);

  assert(bvm::operand::immediate::parse("$-10")->value == uint64_t(-10));
  assert(int64_t(bvm::operand::immediate::parse("$-10")->value) == -10);

  assert(bvm::operand::immediate::parse("$0x-10")->value == uint64_t(-16));
  assert(int64_t(bvm::operand::immediate::parse("$0x-10")->value) == -16);
  assert(bvm::operand::immediate::parse("$0-10")->value == uint64_t(-8));
  assert(int64_t(bvm::operand::immediate::parse("$0-10")->value) == -8);
}

void test_operand_parse() {
  assert(dynamic_cast<bvm::operand::reg_rax*>(bvm::operand::parse("%rax").get()));
  assert(dynamic_cast<bvm::operand::mem_access*>(bvm::operand::parse("(%rax)").get()));
  assert(dynamic_cast<bvm::operand::mem_access*>(bvm::operand::parse("( %rax , %rbx , $8 )").get()));
}

void test_instruction_parse(){
  assert(dynamic_cast<bvm::instruction::add3*>(bvm::instruction::parse("add %rax, %rax, $0").get()));
  assert(dynamic_cast<bvm::instruction::exit*>(bvm::instruction::parse("exit %rax").get()));
  assert(dynamic_cast<bvm::instruction::jmp*>(bvm::instruction::parse("jmp %rax").get()));
  assert(dynamic_cast<bvm::instruction::add3*>(bvm::instruction::parse("add (%rbx,$4,$2), %rax, $0 ").get()));
}

void test_instruction_execution(){
  bvm::machine m;
  bvm::instruction::parse("scan_int %rax")->execute_on(m);
  bvm::instruction::parse("scan_int %rbx")->execute_on(m);
  bvm::instruction::parse("mul %rax, %rbx")->execute_on(m);
  bvm::instruction::parse("print_int %rax")->execute_on(m);
}



int main(){
  test_literal_parse();
  test_operand_parse();
  test_instruction_parse();
  test_instruction_execution();
}