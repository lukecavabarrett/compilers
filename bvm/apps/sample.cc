#include <bvm.h>
#include <parse_asm.h>
#include <iostream>
#include <cassert>
#include <fstream>
#include <thread>

void test_literal_parse(){
  assert(bvm::operand::asm_parse_immediate("$010")->value == 8);
  assert(bvm::operand::asm_parse_immediate("$10")->value == 10);
  assert(bvm::operand::asm_parse_immediate("$0x10")->value == 16);

  assert(bvm::operand::asm_parse_immediate("$-10")->value == uint64_t(-10));
  assert(int64_t(bvm::operand::asm_parse_immediate("$-10")->value) == -10);

  assert(bvm::operand::asm_parse_immediate("$0x-10")->value == uint64_t(-16));
  assert(int64_t(bvm::operand::asm_parse_immediate("$0x-10")->value) == -16);
  assert(bvm::operand::asm_parse_immediate("$0-10")->value == uint64_t(-8));
  assert(int64_t(bvm::operand::asm_parse_immediate("$0-10")->value) == -8);
}

void test_operand_parse() {

  assert(dynamic_cast<bvm::operand::reg_rax*>(bvm::operand::asm_parse("%rax").get()));
  assert(dynamic_cast<bvm::operand::mem_access*>(bvm::operand::asm_parse("(%rax)").get()));
  assert(dynamic_cast<bvm::operand::mem_access*>(bvm::operand::asm_parse("( %rax , %rbx , $8 )").get()));
}

void test_instruction_parse(){
  std::unordered_map<std::string,bvm::word_t> em;
  assert(dynamic_cast<bvm::instruction::add3*>(bvm::instruction::asm_parse("add %rax, %rax, $0",em).get()));
  assert(dynamic_cast<bvm::instruction::exit*>(bvm::instruction::asm_parse("exit %rax",em).get()));
  assert(dynamic_cast<bvm::instruction::jmp*>(bvm::instruction::asm_parse("jmp %rax",em).get()));
  assert(dynamic_cast<bvm::instruction::add3*>(bvm::instruction::asm_parse("add (%rbx,$4,$2), %rax, $0 ",em).get()));
}

void test_instruction_execution(){
  bvm::machine m;
  std::unordered_map<std::string,bvm::word_t> em;
  bvm::instruction::asm_parse("scan_int %rax",em)->execute_on(m);
  bvm::instruction::asm_parse("scan_int %rbx",em)->execute_on(m);
  bvm::instruction::asm_parse("mul %rax, %rbx",em)->execute_on(m);
  bvm::instruction::asm_parse("print_int %rax",em)->execute_on(m);
}

void test_file(){
  std::ifstream filestream("fibonacci.asm");
  std::string filedata((std::istreambuf_iterator<char>(filestream)),
                  std::istreambuf_iterator<char>());
  auto code = bvm::asm_parse_file(filedata);
  std::cerr << "Loaded program of size "<<code.size() << std::endl;
  bvm::machine m;
  std::optional<bvm::word_t> exit_code;
  while(!exit_code.has_value()) {
    exit_code = code[m.r.rip]->execute_on(m);
    ++m.r.rip;
  }
  std::cerr <<"Process finished with exit code "<<exit_code.value()<<std::endl;

}


int main(){
  test_literal_parse();
  test_operand_parse();
  test_instruction_parse();
  //test_instruction_execution();
  test_file();
}