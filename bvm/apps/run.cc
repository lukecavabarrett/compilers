#include <bvm.h>
#include <parse_asm.h>
#include <iostream>
#include <fstream>

int main(int argc, char *argv[]) {

  std::ifstream filestream(argv[1]);
  std::string filedata((std::istreambuf_iterator<char>(filestream)),
                       std::istreambuf_iterator<char>());
  std::cerr << "[ Assebling source of size " << filedata.size() << " bytes ]" << std::endl;
  auto code = bvm::asm_parse_file(filedata);
  std::cerr << "[ Running generated executable ]" << std::endl;
  bvm::machine m;
  std::optional<bvm::word_t> exit_code;
  while (!exit_code.has_value()) {
    exit_code = code[m.r.rip]->execute_on(m);
    ++m.r.rip;
  }
  std::cerr << "[ Process finished with exit code " << exit_code.value() << " ]" << std::endl;
}