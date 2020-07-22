#ifndef COMPILERS_BVM_LIB_PARSE_ASM_H_
#define COMPILERS_BVM_LIB_PARSE_ASM_H_

#include <bvm.h>
#include <algorithm>
#include <cassert>

namespace bvm {

namespace operand {

std::unique_ptr<mem_access> asm_parse_mem_access(std::string_view s);
std::unique_ptr<immediate> asm_parse_immediate(std::string_view s);
std::unique_ptr<reg> asm_parse_reg(std::string_view s);
std::unique_ptr<t> asm_parse(std::string_view s,const std::unordered_map<std::string, word_t>& l);
std::unique_ptr<t> asm_parse(std::string_view s);

}

namespace instruction {
std::unique_ptr<t> asm_parse(std::string_view s,const std::unordered_map<std::string, word_t>& l);

}



std::vector<std::unique_ptr<instruction::t> > asm_parse_file(std::string_view s);



}

#endif //COMPILERS_BVM_LIB_PARSE_ASM_H_
