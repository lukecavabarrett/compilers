#ifndef COMPILERS_BML_LIB_BUILDER_BUILD_H_
#define COMPILERS_BML_LIB_BUILDER_BUILD_H_

#include <parse/parse.h>

void resolve_global_free_vars(ast::free_vars_t &&fv, const ast::global_map &m) {
  for (auto&[name, usages] : fv)
    if (auto it = m.find(name); it != m.end()) {
      for (auto id : usages)id->definition_point = it->second;
      it->second->usages.merge(std::move(usages));
    } else throw ast::unbound_value((*std::min_element(usages.begin(), usages.end(), [](const auto &i1, const auto &i2) { return i1->name.begin() < i2->name.begin(); }))->name);
}

void build(std::string_view s) {

#define target  "/home/luke/CLionProjects/compilers/bml/output"

  parse::tokenizer tk(s);
  ast::global_map globals;
  ast::matcher::universal_matcher int_sum("int_sum");
  int_sum.glo_fun_name = int_sum.top_level = true;
  globals.try_emplace("int_sum", &int_sum);
  globals.try_emplace("+",&int_sum);
  ast::matcher::universal_matcher int_eq("int_eq");
  int_eq.glo_fun_name = int_eq.top_level = true;
  globals.try_emplace("int_eq", &int_eq);
  globals.try_emplace("=",&int_eq);

  ast::matcher::universal_matcher int_print("int_print");
  int_print.glo_fun_name = int_print.top_level = true;
  globals.try_emplace("int_print", &int_print);
  std::stringstream data_section;
  data_section << "section .data\n"
                  "int_sum dq 1,2,int_sum_fn\n"
                  "int_format db  \"%lld\",10, 0\n"
                  "int_print dq 1,1,int_print_fn\n"
                  "int_eq dq 1,2,int_eq_fn\n" << std::endl;
  std::stringstream text_section;
  text_section << "section .text\nglobal main\nextern printf, malloc\n";
  {
    text_section << "apply_fn:\n"
                    "        push    rbp\n"
                    "        mov     rbp, rdi\n"
                    "        mov     edi, 32\n"
                    "        push    rbx\n"
                    "        mov     rbx, rsi\n"
                    "        sub     rsp, 8\n"
                    "        call    malloc\n"
                    "        mov     rcx, qword [rbp+8]\n"
                    "        mov     qword [rax], 5\n"
                    "        lea     rdx, [rcx-1]\n"
                    "        mov     qword [rax+16], rbp\n"
                    "        mov     qword [rax+8], rdx\n"
                    "        mov     qword [rax+24], rbx\n"
                    "        test    rdx, rdx\n"
                    "        je      .L18\n"
                    "        add     rsp, 8\n"
                    "        pop     rbx\n"
                    "        pop     rbp\n"
                    "        ret\n"
                    ".L7:\n"
                    "        mov     rbp, qword [rbp+16]\n"
                    ".L18:\n"
                    "        mov     rdx, qword [rbp+0]\n"
                    "        cmp     rdx, 5\n"
                    "        je      .L7\n"
                    "        mov     rcx, qword [rbp+16]\n"
                    "        cmp     rdx, 1\n"
                    "        je      .L19\n"
                    "        add     rsp, 8\n"
                    "        mov     rsi, rbp\n"
                    "        mov     rdi, rax\n"
                    "        pop     rbx\n"
                    "        pop     rbp\n"
                    "        jmp     rcx\n"
                    ".L19:\n"
                    "        add     rsp, 8\n"
                    "        mov     rdi, rax\n"
                    "        pop     rbx\n"
                    "        pop     rbp\n"
                    "        jmp     rcx\n"
                    "int_sum_fn:\n"
                    "        mov     rax, qword [rdi+16]\n"
                    "        mov     rdx, qword [rdi+24]\n"
                    "        mov     rax, qword [rax+24]\n"
                    "        sar     rdx, 1\n"
                    "        sar     rax, 1\n"
                    "        add     rax, rdx\n"
                    "        lea     rax, [rax+1+rax]\n"
                    "        ret\n"
                    "\n"
                    "int_eq_fn:\n"
                    "        mov     rax, qword [rdi+16]\n"
                    "        mov     rdx, qword [rdi+24]\n"
                    "        mov     rax, qword [rax+24]\n"
                    "        sar     rdx, 1\n"
                    "        sar     rax, 1\n"
                    "        cmp     rax, rdx\n"
                    "        jnz .unequal     \n"
                    "        mov rax, 3 \n"
                    "        ret         \n"
                    ".unequal:\n"
                    "        mov     rax, 1\n"
                    "        ret\n"
                    "\n"
                    "int_print_fn:\n"
                    "        sub     rsp, 8\n"
                    "        mov     rsi, qword [rdi+24]\n"
                    "        xor     eax, eax\n"
                    "        mov     edi, int_format\n"
                    "        sar     rsi, 1\n"
                    "        call    printf\n"
                    "        xor     eax, eax\n"
                    "        add     rsp, 8\n"
                    "        ret\n";
  }
  std::stringstream main_section;
  main_section << "main:" << std::endl;
  std::vector<ast::definition::ptr> defs;
  while (true) {
    if (tk.empty())break;
    if (tk.peek() == parse::LET) {
      //value definition
      try {
        auto def = ast::definition::parse(tk);
        tk.expect_pop(parse::EOC);

        ast::free_vars_t fv = def->free_vars();
        resolve_global_free_vars(std::move(fv), globals);
        for (auto &def : def->defs)def->binder().globally_register(globals);
        auto cg = def->capture_group();
        if(!cg.empty()){

          std::cerr << cg.size() << std::endl;
          for(auto& m : cg){
            std::cerr << m->name << std::endl;
          }
        }
        assert(cg.empty());
        def->compile_global(util::sections_t(data_section,text_section,main_section));
        defs.push_back(std::move(def));

      } catch (const util::error::message &e) {
        e.print(std::cout, s, "source.ml");
        exit(1);
      }

      //THROW_UNIMPLEMENTED
    } else if (tk.peek() == parse::TYPE) {
      //type definition
      THROW_UNIMPLEMENTED
    } else {
      THROW_UNIMPLEMENTED
    }
  }
  main_section << "xor eax, eax\n"
                  "ret\n";
  std::cout << data_section.str() << std::endl;
  std::cout << text_section.str() << std::endl;
  std::cout << main_section.str() << std::endl;

  std::ofstream oasm;
  oasm.open(target".asm");
  oasm << data_section.str() << std::endl << text_section.str() << std::endl << main_section.str() << std::endl;
  oasm.close();

  assert(system("yasm -g dwarf2 -f elf64 " target ".asm -l " target ".lst -o " target ".o")==0);
  assert(system("gcc -no-pie " target ".o -o " target)==0);
  int ret = system(target);
  std::cout << "Exited: " << WEXITSTATUS(ret) << std::endl;

  return;
}

#endif //COMPILERS_BML_LIB_BUILDER_BUILD_H_
