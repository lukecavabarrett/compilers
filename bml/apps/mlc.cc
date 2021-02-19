#include <build/build.h>
#include <util/util.h>
#include <cstring>

std::string_view get_arg(int argc, const char *argv[], std::string_view argname, std::string_view on_fail) {
  auto it = std::find_if(argv, argv + argc, [argname](const char *p) { return std::strcmp(p, argname.data()) == 0; });
  if (it == argv + argc)return on_fail;
  if (it == argv + argc - 1)return on_fail;
  return it[1];
}

int main(int argc, const char *argv[]) {
  std::string_view source_path = get_arg(argc, argv, argv[0], "/tmp/file.asm");
  std::string_view target_asm = get_arg(argc, argv, "-oasm", "/tmp/file.asm");
  std::string_view target_obj = get_arg(argc, argv, "-oobj", "/tmp/file.o");
  std::string_view target = get_arg(argc, argv, "-o", "/tmp/file.exe");
  std::string_view
      lib_object_file = get_arg(argc, argv, "-lib", "/home/luke/CLionProjects/compilers/bml/lib/rt/rt_fast.o");
  std::string source = util::load_file(source_path);
  std::ofstream oasm;
  oasm.open(target_asm.data());
  try {
    build_ir(source, oasm, source_path);
  } catch (const std::exception &) {
    return 1;
  }
  oasm.close();
  //YASM
  std::string yasm_command;
  (yasm_command = "yasm -g dwarf2 -f elf64 ").append(target_asm).append(" -o ").append(target_obj);
  assert(system(yasm_command.c_str()) == 0);
  //LINK
  std::string link_command;
  (link_command = "gcc -no-pie ").append(target_obj).append(" ").append(lib_object_file).append(" -o ").append(target);
  assert(system(link_command.c_str()) == 0);

}
