#ifndef COMPILERS_BML_LIB_BUILDER_BUILD_H_
#define COMPILERS_BML_LIB_BUILDER_BUILD_H_

#include <parse/parse.h>

void resolve_global_free_vars(ast::free_vars_t &&fv, const ast::global_map &m) {
  for (auto&[name, usages] : fv){
    if (auto it = m.find(name); it != m.end()) {
      for (auto id : usages)id->definition_point = it->second;
      it->second->usages.merge(std::move(usages));
    } else throw ast::unbound_value((*std::min_element(usages.begin(), usages.end(), [](const auto &i1, const auto &i2) { return i1->name.begin() < i2->name.begin(); }))->name);
  }
}

void build(std::string_view s, std::ostream &target) {

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

  ast::constr_map constr_map;

  std::stringstream data_section;
  data_section << std::ifstream("/home/luke/CLionProjects/compilers/bml/lib/build/data_preamble.asm").rdbuf();
  std::stringstream text_section;
  text_section << std::ifstream("/home/luke/CLionProjects/compilers/bml/lib/build/text_preamble.asm").rdbuf();
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
        def->compile_global(util::sections_t(data_section, text_section, main_section));
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
              constr_map.try_emplace(v.name,&v);
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

/*
 IDEA for tests:
 1. let (a,b) = fun () -> 3 ;;  // Error: This expression should not be a function, the expected type is 'a * 'b
 2. let rec x = a () and y = 3 + 4 and z = Some x ;; // OK
 Int a let rec, every right hand side should be either free from siblings or 
 */

#endif //COMPILERS_BML_LIB_BUILDER_BUILD_H_
