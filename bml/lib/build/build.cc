#include <build.h>

struct ir_registerer_t {
  template<size_t Id>
  void add(std::string_view external_name,
           size_t args,
           std::string_view name,
           std::initializer_list<std::string_view> aliases = {}) {
    static ast::matcher::universal m(name);
    data << "extern " << external_name << " \n";
    m.ir_globally_register(globals);
    m.ir_allocate_globally_funblock(data, args, external_name);
    m.use_as_immediate = m.top_level = true;
    for (std::string_view alias : aliases)
      globals.try_emplace(alias, &m); // alias maps to the same
  }
  ast::global_map &globals;
  std::ostream &data;
};
ast::global_map make_ir_data_section(std::ostream &target) {
  //prepare all the standard data
  ast::global_map globals;
  target << "section .data\n";
  target << "extern apply_fn, decrement_nontrivial, decrement_value, increment_value, malloc, json_debug\n";

  ir_registerer_t ir_registerer{.globals=globals, .data=target};

#define register ir_registerer.add<__COUNTER__>

  register("_mllib_fn__int_add", 2, "__binary_op__PLUS__");
  register("_mllib_fn__int_sub", 2, "__binary_op__MINUS__");
  register("_mllib_fn__int_mul", 2, "__binary_op__STAR__");
  register("_mllib_fn__int_div", 2, "__binary_op__SLASH__");
  register("_mllib_fn__int_neg", 1, "__unary_op__MINUS__");
  register("_mllib_fn__int_eq", 2, "__binary_op__EQUAL__");
  register("_mllib_fn__int_lt", 2, "__binary_op__LESS_THAN__");
  register("_mllib_fn__int_leq", 2, "__binary_op__LESS_EQUAL_THAN__");
  register("_mllib_fn__int_gt", 2, "__binary_op__GREATER_THAN__");
  register("_mllib_fn__int_geq", 2, "__binary_op__GREATER_EQUAL_THAN__");
  register("_mllib_fn__int_print", 1, "print_int");
  register("_mllib_fn__int_println", 1, "println_int");
  register("_mllib_fn__int_fprintln", 2, "fprintln_int");
  register("_mllib_fn__int_scan", 1, "scan_int");
  register("_mllib_fn__str_print", 1, "print_str");
  register("_mllib_fn__str_fprint", 2, "fprint_str");
  register("_mllib_fn__str_length", 1, "strlen");
  register("_mllib_fn__chr_print", 1, "print_chr");
  register("_mllib_fn__str_at", 2, "str_at");
  register("_mllib_fn__fopen", 2, "fopen");
  register("_mllib_fn__fclose", 1, "fclose");
  register("_mllib_fn__t_deep_copy", 1, "deep_copy");

#undef register

  //special case for the unmatched function
  {
    target << "extern match_failed_fun \n";
    static ast::matcher::universal mf("__throw__unmatched__");
    mf.ir_globally_register(globals);
    mf.ir_allocate_globally_funblock(target, 1, "match_failed_fun");
    mf.use_as_immediate = mf.top_level = true;
    target << "__throw__unmatched__ equ " << mf.ir_asm_name() << "\n";
  }
  //IO
  //TODO-someday: use more constexpr representation for stderr, stdout
  {
    static ast::matcher::universal mf("stdout");
    mf.ir_globally_register(globals);
    mf.use_as_immediate = mf.top_level = true;
    target << mf.ir_asm_name() << " equ 3" << "; stdout\n";
  }
  {
    static ast::matcher::universal mf("stderr");
    mf.ir_globally_register(globals);
    mf.use_as_immediate = mf.top_level = true;
    target << mf.ir_asm_name() << " equ 5" << "; stderr\n";
  }

  return globals;
}

void build_ir(std::string_view s, std::ostream &target, std::string_view filename) {
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
              main.comment() << " Tag \"" << v.name << "\" = " << v.tag;
              //data_section << "; " << v.name << " equ " << v.tag << "\n";
            }
          }
        type_defs.push_back(std::move(tp));
      } catch (const util::error::message &e) {
        e.print(std::cout, s, "source.ml");
        throw std::runtime_error("compilation error");
      }
    } else {
      //expression
      try {
        auto e = ast::expression::parse(tk);
        tk.expect_pop(parse::EOC);
        e->bind(constr_map);
        ast::free_vars_t fv = e->free_vars();
        resolve_global_free_vars(std::move(fv), globals);
        auto cg = e->capture_group();
        assert(cg.empty());
        e->ir_compile(ir_sections_t(target, std::back_inserter(functions), main));

      } catch (const util::error::message &e) {
        e.print(std::cout, s, "source.ml");
        throw std::runtime_error("compilation error");
      }
    }
  }
  target << "global main\n" "section .text\n";
  main.ret = main.declare_constant(0);
  functions.push_back(std::move(main));
  for (auto &f : functions) {
    //f.pre_compile();
    //f.print(std::cout);
    f.compile(target);
  }
}

struct direct_registerer_t {
  template<size_t Id>
  void add(std::string_view name, std::initializer_list<std::string_view> aliases = {}) {
    static ast::matcher::universal m(name);

    m.use_as_immediate = m.top_level = true;
    globals.try_emplace(name, &m);
    for (std::string_view alias : aliases)
      globals.try_emplace(alias, &m);
  }
  ast::global_map &globals;
};
void build_direct(std::string_view s, std::ostream &target) {

  parse::tokenizer tk(s);
  ast::global_map globals;

  direct_registerer_t direct_registerer{.globals=globals};

#define register direct_registerer.add<__COUNTER__>
  register("int_sum", {"__binary_op__PLUS__"});
  register("int_sub", {"__binary_op__MINUS__"});
  register("int_eq", {"__binary_op__EQUAL__"});
  register("print_int");
  register("println_int");
  register("int_le", {"__binary_op__LESS_THAN__"});
  register("int_negate", {"__unary_op__MINUS__"});
#undef register

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
      //expression
      try {
        auto e = ast::expression::parse(tk);
        tk.expect_pop(parse::EOC);
        e->bind(constr_map);
        ast::free_vars_t fv = e->free_vars();
        resolve_global_free_vars(std::move(fv), globals);
        auto cg = e->capture_group();
        assert(cg.empty());
        e->compile(util::direct_sections_t(data_section, text_section, main_section), 0);
      } catch (const util::error::message &e) {
        e.print(std::cout, s, "source.ml");
        throw std::runtime_error("compilation error");
      }
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
                                                    return i1->loc.begin() < i2->loc.begin();
                                                  }))->loc);
    //TODO: decide whether it would be nicer to report all occurrences of the unbound variable
  }
}
