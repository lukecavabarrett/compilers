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
  register("_mllib_fn__time_print", 1, "print_time");
  register("_mllib_fn__time_fprint", 2, "fprint_time");
  register("_mllib_fn__time_now", 1, "now");
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

void record_typedef(type::constr_map &constr_map,
                    std::vector<type::function::variant::ptr> &variants,
                    type::type_map &type_map,
                    ast::type::definition::ptr &&tp) {

  if (std::any_of(tp->defs.begin(), tp->defs.end(), [](const auto &p) {
    return dynamic_cast<ast::type::definition::single_variant *>(p.get()) == nullptr;
  }))
    THROW_UNIMPLEMENTED; //only variants are allowed
  assert(tp->nonrec == false);//"Not implemented yet";
  //TODO: ensure declared names are all different
  if (!tp->nonrec) {
    //load variant scratches in, so that they can be reused
    for (const auto &p : tp->defs) {
      variants.push_back(std::make_unique<type::function::variant>(p->name, p->params.size()));
      type_map[p->name] = variants.back().get();
    }
  }
  {
    size_t j = 0;
    for (const auto &p : tp->defs) {
      //build the options
      const auto *sv = dynamic_cast<ast::type::definition::single_variant *>(p.get());
      type::function::variant::ptr &pv = variants.at(variants.size() - tp->defs.size() + j);
      assert(p->name == pv->name);
      std::unordered_map<std::string_view,size_t> params_id;
      if(p->name.at(0)=='\'')throw parse::error::report_token(p->name,"declared type name","cannot start with ' (tick)");
      for(size_t i = 0; i < sv->params.size();++i){
        std::string_view p = sv->params.at(i)->name;
        if(p.at(0)!='\'')throw parse::error::report_token(p,"type parameter","doesn't start with ' (tick)");
        if(!params_id.try_emplace(p,i).second){
          throw parse::error::report_token(p,"type parameter","occurred more than once. this is illegal");
        }
      }
      for (const auto &ac : sv->variants) {
        type::function::variant::constr c(ac.name,pv.get());
        for(const auto &a : ac.args){
          c.args.push_back(a->to_type(params_id,type_map));
        }
        pv->constructors.push_back(std::move(c));
      }
      for(const auto& c : pv->constructors){
        constr_map.try_emplace(c.name,&c);
      }

      ++j;
    }
  }
//TODO: check ownership of constructors
}

void build_ir(std::string_view s, std::ostream &target, std::string_view filename) {
  parse::tokenizer tk(s);
  ast::global_map globals = make_ir_data_section(target);
  type::type_map type_map = type::make_default_type_map();

  type::constr_map constr_map;
  std::vector<ir::lang::function> functions;
  std::vector<ast::definition::ptr> defs;
  //std::vector<ast::type::definition::ptr> type_defs;
  std::vector<type::function::variant::ptr> variants;
  ir::lang::function main;
  main.name = "main";
  while (!tk.empty()) {
    while (tk.peek() == parse::EOC)tk.pop();
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
    } else if (tk.peek() == parse::TYPE) {
      //type definition
      try {
        auto tp = ast::type::definition::parse(tk);
        tk.expect_pop(parse::EOC);
        record_typedef(constr_map, variants, type_map, std::move(tp));
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
//    f.pre_compile();
//    f.print(std::cout);
    f.compile(target);
  }
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
