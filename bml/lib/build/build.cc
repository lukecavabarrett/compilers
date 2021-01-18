#include <build.h>


ast::global_map make_ir_data_section(std::ostream &target) {
  //prepare all the standard data
  ast::global_map globals;
  target << "section .data\n";
  target << "extern apply_fn, decrement_nontrivial, decrement_value, increment_value, malloc, json_debug\n";

  {
    target << "extern int_sum_fun \n";
    static ast::matcher::universal_matcher int_sum("int_sum");
    int_sum.ir_globally_register(globals);
    int_sum.ir_allocate_globally_funblock(target, 2, "int_sum_fun");
    int_sum.use_as_immediate = int_sum.top_level = true;
    globals.try_emplace("+", &int_sum); // + maps to the same
  }

  {
    target << "extern int_sub_fun \n";
    static ast::matcher::universal_matcher int_sub("int_sub");
    int_sub.ir_globally_register(globals);
    int_sub.ir_allocate_globally_funblock(target, 2, "int_sub_fun");
    int_sub.use_as_immediate = int_sub.top_level = true;
    globals.try_emplace("-", &int_sub); // - maps to the same
  }

  {
    target << "extern print_int \n";
    static ast::matcher::universal_matcher int_print("int_print");
    int_print.ir_globally_register(globals);
    int_print.ir_allocate_globally_funblock(target, 1, "print_int");
    int_print.use_as_immediate = int_print.top_level = true;
  }

  {
    target << "extern println_int \n";
    static ast::matcher::universal_matcher int_println("int_println");
    int_println.ir_globally_register(globals);
    int_println.ir_allocate_globally_funblock(target, 1, "println_int");
    int_println.use_as_immediate = int_println.top_level = true;
  }

  {
    target << "extern int_eq_fun \n";
    static ast::matcher::universal_matcher int_eq("int_eq");
    int_eq.ir_globally_register(globals);
    int_eq.ir_allocate_globally_funblock(target, 2, "int_eq_fun");
    int_eq.use_as_immediate = int_eq.top_level = true;
    globals.try_emplace("=", &int_eq);
  }

  {
    target << "extern match_failed_fun \n";
    static ast::matcher::universal_matcher mf("__throw__unmatched__");
    mf.ir_globally_register(globals);
    mf.ir_allocate_globally_funblock(target, 1, "match_failed_fun");
    mf.use_as_immediate = mf.top_level = true;
    target << "__throw__unmatched__ equ " << mf.ir_asm_name() << "\n";
  }

  /*static ast::matcher::universal_matcher int_sub("int_sub");
  int_sub.use_as_immediate = int_sub.top_level = true;
  globals.try_emplace("int_sub", &int_sub);
  globals.try_emplace("-", &int_sub);

  static ast::matcher::universal_matcher int_eq("int_eq");
  int_eq.use_as_immediate = int_eq.top_level = true;
  globals.try_emplace("int_eq", &int_eq);
  globals.try_emplace("=", &int_eq);

  static ast::matcher::universal_matcher int_print("int_print");
  int_print.use_as_immediate = int_print.top_level = true;
  globals.try_emplace("int_print", &int_print);

  static ast::matcher::universal_matcher int_println("int_println");
  int_println.use_as_immediate = int_println.top_level = true;
  globals.try_emplace("int_println", &int_println);

  static ast::matcher::universal_matcher int_le("int_le");
  int_le.use_as_immediate = int_le.top_level = true;
  globals.try_emplace("int_le", &int_le);*/

  return globals;
}

void build_ir(std::string_view s, std::ostream &target) {
  parse::tokenizer tk(s);
  ast::global_map globals = make_ir_data_section(target);

  ast::constr_map constr_map;
  std::vector<ir::lang::function> functions;
  std::vector<ast::definition::ptr> defs;
  std::vector<ast::type::definition::ptr> type_defs;
  ir::lang::function main;
  main.name = "main";
  while (true) {
    if (tk.empty())break;
    if (tk.peek() == parse::LET) {
      //value definition
      try {
        auto def = ast::definition::parse(tk);
        tk.expect_pop(parse::EOC);

        def->bind(constr_map);
        ast::free_vars_t fv = def->free_vars();
        resolve_global_free_vars(std::move(fv), globals);
        for (auto &def : def->defs)def.name->ir_globally_register(globals);
        auto cg = def->capture_group();
        assert(cg.empty());
        def->ir_compile_global(ir_sections_t(target, std::back_inserter(functions), main));
        defs.push_back(std::move(def));

      } catch (const util::error::message &e) {
        e.print(std::cout, s, "source.ml");
        throw std::runtime_error("compilation error");
      }

      //THROW_UNIMPLEMENTED
    } else if (tk.peek() == parse::TYPE) {
      //type definition
      try {
        auto tp = ast::type::definition::parse(tk);
        tk.expect_pop(parse::EOC);
        for (auto &td : tp->defs)
          if (auto *sv = dynamic_cast<ast::type::definition::single_variant *>(td.get())) {
            uint64_t tag_id = 5;
            for (auto &v : sv->variants) {
              v.tag = tag_id;
              tag_id += 2;
              constr_map.try_emplace(v.name, &v);
              //data_section << "; " << v.name << " equ " << v.tag << "\n";
            }
          }
        type_defs.push_back(std::move(tp));
      } catch (const util::error::message &e) {
        e.print(std::cout, s, "source.ml");
        throw std::runtime_error("compilation error");
      }
    } else {
      THROW_UNIMPLEMENTED
    }
  }
  target << "global main\n" "section .text\n";
  main.ret = main.declare_constant(0);
  functions.push_back(std::move(main));
  for (auto &f : functions) {
    f.pre_compile();
    f.print(std::cout);
    f.compile(target);
  }
}
void build_direct(std::string_view s, std::ostream &target) {

  parse::tokenizer tk(s);
  ast::global_map globals;
  ast::matcher::universal_matcher int_sum("int_sum");
  int_sum.use_as_immediate = int_sum.top_level = true;
  globals.try_emplace("int_sum", &int_sum);
  globals.try_emplace("+", &int_sum);

  ast::matcher::universal_matcher int_sub("int_sub");
  int_sub.use_as_immediate = int_sub.top_level = true;
  globals.try_emplace("int_sub", &int_sub);
  globals.try_emplace("-", &int_sub);

  ast::matcher::universal_matcher int_eq("int_eq");
  int_eq.use_as_immediate = int_eq.top_level = true;
  globals.try_emplace("int_eq", &int_eq);
  globals.try_emplace("=", &int_eq);

  ast::matcher::universal_matcher int_print("int_print");
  int_print.use_as_immediate = int_print.top_level = true;
  globals.try_emplace("int_print", &int_print);

  ast::matcher::universal_matcher int_println("int_println");
  int_println.use_as_immediate = int_println.top_level = true;
  globals.try_emplace("int_println", &int_println);

  ast::matcher::universal_matcher int_le("int_le");
  int_le.use_as_immediate = int_le.top_level = true;
  globals.try_emplace("int_le", &int_le);

  ast::constr_map constr_map;

  std::stringstream data_section;
  data_section << std::ifstream("/home/luke/CLionProjects/compilers/bml/lib/build/data_preamble_direct.asm").rdbuf();
  std::stringstream text_section;
  text_section << std::ifstream("/home/luke/CLionProjects/compilers/bml/lib/build/text_preamble_direct.asm").rdbuf();
  std::stringstream main_section;
  main_section << "main:" << std::endl;
  std::vector<ast::definition::ptr> defs;
  std::vector<ast::type::definition::ptr> type_defs;
  while (true) {
    if (tk.empty())break;
    if (tk.peek() == parse::LET) {
      //value definition
      try {
        auto def = ast::definition::parse(tk);
        tk.expect_pop(parse::EOC);

        def->bind(constr_map);
        ast::free_vars_t fv = def->free_vars();
        resolve_global_free_vars(std::move(fv), globals);
        for (auto &def : def->defs)def.name->globally_register(globals);
        auto cg = def->capture_group();
        assert(cg.empty());
        def->compile_global(util::direct_sections_t(data_section, text_section, main_section));
        defs.push_back(std::move(def));

      } catch (const util::error::message &e) {
        e.print(std::cout, s, "source.ml");
        throw std::runtime_error("compilation error");
      }

      //THROW_UNIMPLEMENTED
    } else if (tk.peek() == parse::TYPE) {
      //type definition
      try {
        auto tp = ast::type::definition::parse(tk);
        tk.expect_pop(parse::EOC);
        for (auto &td : tp->defs)
          if (auto *sv = dynamic_cast<ast::type::definition::single_variant *>(td.get())) {
            uint64_t tag_id = 1;
            for (auto &v : sv->variants) {
              v.tag = tag_id;
              tag_id += 2;
              constr_map.try_emplace(v.name, &v);
              //data_section << "; " << v.name << " equ " << v.tag << "\n";
            }
          }
        type_defs.push_back(std::move(tp));
      } catch (const util::error::message &e) {
        e.print(std::cout, s, "source.ml");
        throw std::runtime_error("compilation error");
      }
    } else {
      THROW_UNIMPLEMENTED
    }
  }
  main_section << "xor eax, eax\n"
                  "ret\n";

  target << data_section.str() << std::endl << text_section.str() << std::endl << main_section.str() << std::endl;

}
void resolve_global_free_vars(ast::free_vars_t &&fv, const ast::global_map &m) {
  for (auto&[name, usages] : fv) {
    if (auto it = m.find(name); it != m.end()) {
      for (auto id : usages)id->definition_point = it->second;
      it->second->usages.merge(std::move(usages));
    } else
      throw ast::unbound_value((*std::min_element(usages.begin(),
                                                  usages.end(),
                                                  [](const auto &i1, const auto &i2) {
                                                    return i1->name.begin() < i2->name.begin();
                                                  }))->name);
  }
}
