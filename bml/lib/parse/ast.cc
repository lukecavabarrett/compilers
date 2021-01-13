#include <ast.h>
#include <util/sexp.h>
#include <parse.h>

namespace ast {

namespace {

free_vars_t join_free_vars(free_vars_t &&v1, free_vars_t &&v2) {
  if (v1.size() < v2.size())std::swap(v1, v2);
  for (auto&[k, l] : v2) {
    if (auto it = v1.find(k);it == v1.end()) {
      v1.try_emplace(k, std::move(l));
    } else {
      it->second.splice_after(it->second.cbefore_begin(), std::move(l));
    }
  }

  return v1;
}

capture_set join_capture_set(capture_set &&c1, capture_set &&c2) {
  if (c1.size() < c2.size())std::swap(c1, c2);
  c1.merge(std::move(c2));
  return c1;
}

}

namespace expression {

free_vars_t identifier::free_vars() {
  std::string n(name);
  if (definition_point && definition_point->top_level) return {};
  return {{name, {this}}};
}

capture_set identifier::capture_group() {
  if (!definition_point) {
    std::string_view n = name;
  }
  if (!definition_point)THROW_INTERNAL_ERROR;
  if (definition_point->top_level) return {};
  return {definition_point};
}
void identifier::compile(direct_sections_t s, size_t stack_pos) {
  std::string_view n = name;
  if (definition_point->top_level) {
    definition_point->globally_evaluate(s.main);
  } else if (s.def_fun && s.def_fun->is_capturing(definition_point)) {
    std::size_t idx = s.def_fun->capture_index(definition_point);
    s.main << "mov rax, qword [r12+" << 8 * (idx + 4) << "]; retrieving " << name << ", #capture " << (idx + 1)
           << " of " << s.def_fun->captures.size() << "\n";
  } else {

    s.main << "mov rax, qword [rsp";
    if (stack_pos > definition_point->stack_relative_pos) {
      s.main << "+" << 8 * (stack_pos - definition_point->stack_relative_pos);
    }
    s.main << "]  ; retrieving " << name << " from local scope\n";
  }
}
identifier::identifier(std::string_view n) : t(n), name(n), definition_point(nullptr) {}
void identifier::bind(const constr_map &) {}
ir::lang::var identifier::ir_compile(ir_sections_t s) {
  if (definition_point->top_level) {
    return definition_point->ir_evaluate_global(s.main);
  } else {
    return definition_point->ir_var;
  }
}

literal::literal(ast::literal::ptr &&v) : value(std::move(v)) {}
free_vars_t literal::free_vars() { return {}; }
capture_set literal::capture_group() { return {}; }
void literal::compile(direct_sections_t s, size_t stack_pos) {
  s.main << "mov rax, " << std::to_string(value->to_value()) << std::endl;
}
ir::lang::var literal::ir_compile(ir_sections_t s) {
  return s.main.declare_constant(this->value->to_value());
}
free_vars_t constructor::free_vars() {
  if (arg)return arg->free_vars();
  return {};
}
capture_set constructor::capture_group() {
  if (arg)arg->capture_group();
  return {};
}
constructor::constructor(std::string_view n) : name(n) {}
void constructor::compile(direct_sections_t s, size_t stack_pos) {
  assert(definition_point);
  if (definition_point->is_immediate() != (arg == nullptr)) {
    (definition_point->is_immediate() ? throw constructor_shouldnt_take_arg(arg->loc)
                                      : throw constructor_should_take_arg(name));
  }
  assert(definition_point->is_immediate() == (arg == nullptr)); // this would be clearly a type-system break
  if (definition_point->is_immediate()) {
    s.main << "mov rax, " << definition_point->tag << " ; == Tag `" << name << "` (immediate)\n";
  } else {
    arg->compile(s, stack_pos); // arg in rax
    s.main << "mov rbx, rax\n" // arg in rbx
           << "mov edi, 16\n"  // 16 to allocate
           << "call malloc\n"  // malloc, block in rax
           << "mov qword [rax], " << definition_point->tag << " ; == Tag `" << name << "` (block)\n"
           << "mov qword [rax+8], rbx\n";
  }
}
void constructor::bind(const constr_map &cm) {
  if (auto it = cm.find(name); it == cm.end()) {
    throw ast::unbound_constructor(name);
  } else {
    definition_point = it->second;
  }
  if (arg)arg->bind(cm);
}
ir::lang::var constructor::ir_compile(ir_sections_t) {
  THROW_UNIMPLEMENTED
}
if_then_else::if_then_else(ptr &&condition, ptr &&true_branch, ptr &&false_branch)
    : condition(std::move(condition)), true_branch(std::move(true_branch)), false_branch(std::move(false_branch)) {}
free_vars_t if_then_else::free_vars() {
  return join_free_vars(join_free_vars(condition->free_vars(),
                                       true_branch->free_vars()),
                        false_branch->free_vars());
}
capture_set if_then_else::capture_group() {
  return join_capture_set(join_capture_set(condition->capture_group(),
                                           true_branch->capture_group()),
                          false_branch->capture_group());
}
void if_then_else::compile(direct_sections_t s, size_t stack_pos) {
  static size_t if_id = 0;
  ++if_id;
  condition->compile(s, stack_pos);
  s.main << "cmp rax, 1\n"
            "je .L_IF_F_" << if_id << "\n";
  true_branch->compile(s, stack_pos);
  s.main << "jmp .L_IF_E_" << if_id << "\n";
  s.main << " .L_IF_F_" << if_id << ":\n";
  false_branch->compile(s, stack_pos);
  s.main << " .L_IF_E_" << if_id << ":\n";

}
void if_then_else::bind(const constr_map &cm) {
  condition->bind(cm);
  true_branch->bind(cm);
  false_branch->bind(cm);
}
ir::lang::var if_then_else::ir_compile(ir_sections_t s) {
  using namespace ir::lang;
  s.main.push_back(instruction::cmp_vars{ .v1 = condition->ir_compile(s), .v2 = s.main.declare_constant(2), .op=instruction::cmp_vars::test});
  scope true_scope, false_scope;
  true_scope.ret =  true_branch->ir_compile(s.with_main(true_scope));
  false_scope.ret =  false_branch->ir_compile(s.with_main(false_scope));
  return s.main.declare_assign(std::make_unique<ternary>(ternary{.cond = ternary::jmp_instr::jz,.nojmp_branch = std::move(true_scope), .jmp_branch = std::move(false_scope) }));
}

build_tuple::build_tuple(std::vector<expression::ptr> &&args) : args(std::move(args)) {}
free_vars_t build_tuple::free_vars() {
  free_vars_t fv;
  for (auto &p : args)fv = join_free_vars(p->free_vars(), std::move(fv));
  return fv;
}
capture_set build_tuple::capture_group() {
  capture_set fv;
  for (auto &p : args)fv = join_capture_set(p->capture_group(), std::move(fv));
  return fv;
}
void build_tuple::compile(direct_sections_t s, size_t stack_pos) {
  s.main << "push r12\n";
  ++stack_pos;
  s.main << "mov edi, " << std::to_string(args.size() * 8) << "\n"
         << "call malloc\n" << "mov r12, rax\n";;
  for (int i = 0; i < args.size(); ++i) {
    args.at(i)->compile(s, stack_pos);
    s.main << "mov qword [r12" << (i ? std::string("+").append(std::to_string(i * 8)) : "") << "], rax\n";
  }
  s.main << "mov rax, r12\n"
            "pop r12\n";
  --stack_pos;
}
void build_tuple::bind(const constr_map &cm) {
  for (auto &p : args)p->bind(cm);
}
ir::lang::var build_tuple::ir_compile(ir_sections_t) {
  THROW_UNIMPLEMENTED
}
fun_app::fun_app(ptr &&f_, ptr &&x_) : f(std::move(f_)), x(std::move(x_)) {
  loc = unite_sv(f, x);
}
free_vars_t fun_app::free_vars() { return join_free_vars(f->free_vars(), x->free_vars()); }
capture_set fun_app::capture_group() { return join_capture_set(f->capture_group(), x->capture_group()); }
void fun_app::compile(direct_sections_t s, size_t stack_pos) {
  f->compile(s, stack_pos);
  s.main << "push rax\n";
  ++stack_pos;
  x->compile(s, stack_pos);
  s.main << "pop rdi\n";
  --stack_pos;
  s.main << "mov rsi, rax\n"
            "call apply_fn\n";
}
void fun_app::bind(const constr_map &cm) {
  f->bind(cm);
  x->bind(cm);
}
ir::lang::var fun_app::ir_compile(ir_sections_t s) {
  using namespace ir::lang;
  var vf = f->ir_compile(s);
  var vx = x->ir_compile(s);
  return s.main.declare_assign(rhs_expr::apply_fn{.f = vf, .x = vx});
}
seq::seq(ptr &&a, ptr &&b) : a(std::move(a)), b(std::move(b)) { loc = unite_sv(this->a, this->b); }
free_vars_t seq::free_vars() { return join_free_vars(a->free_vars(), b->free_vars()); }
capture_set seq::capture_group() { return join_capture_set(a->capture_group(), b->capture_group()); }
void seq::compile(direct_sections_t s, size_t stack_pos) {
  a->compile(s, stack_pos);
  b->compile(s, stack_pos);
}
void seq::bind(const constr_map &cm) {
  a->bind(cm);
  b->bind(cm);
}
ir::lang::var seq::ir_compile(ir_sections_t s) {
  a->ir_compile(s);
  return b->ir_compile(s);
}

match_with::match_with(expression::ptr &&w) : what(std::move(w)) {}
free_vars_t match_with::free_vars() {
  free_vars_t fv = what->free_vars();
  for (auto&[p, r] : branches) {
    free_vars_t bfv = r->free_vars();
    p->bind(bfv);
    fv = join_free_vars(std::move(fv), std::move(bfv));
  }
  return fv;
}
capture_set match_with::capture_group() {
  capture_set cs = what->capture_group();
  for (auto&[p, r] : branches) {
    capture_set bcs = r->capture_group();
    p->bind(bcs);
    cs = join_capture_set(std::move(cs), std::move(bcs));
  }
  return cs;
}
void match_with::compile(direct_sections_t s, size_t stack_pos) {
  static size_t match_id = 0;
  ++match_id;
  what->compile(s, stack_pos);
  s.main << "push rax\n";
  ++stack_pos;
  size_t branch_id = 0;
  for (auto&[p, r] : branches) {
    if (branch_id) {
      s.main << ".MATCH_WITH_" << match_id << "_" << branch_id << "\n";
      s.main << "mov rax, qword [rsp]\n";
    }
    std::string on_fail = (branch_id < branches.size() - 1)
                          ? std::string(".MATCH_WITH_").append(std::to_string(match_id)).append("_").append(std::to_string(
            branch_id + 1)) : "fail_match";
    s.main << "; case " << p->to_texp()->to_string() << "\n";
    size_t successful_stack_pos = p->test_locally_unroll(s.main, stack_pos, stack_pos, on_fail);
    if (successful_stack_pos > stack_pos)
      s.main << "sub rsp, " << 8 * (successful_stack_pos - stack_pos) << "; confirming space for accepted pattern\n";
    r->compile(s, successful_stack_pos);
    if (successful_stack_pos > stack_pos)
      s.main << "add rsp, " << 8 * (successful_stack_pos - stack_pos) << "; reclaiming pattern space\n";
    s.main << "jmp .MATCH_WITH_" << match_id << "_END\n";

    ++branch_id;
  }
  s.main << ".MATCH_WITH_" << match_id << "_END\n";
  s.main << "add rsp, 8\n";
  --stack_pos;
}
void match_with::bind(const constr_map &cm) {
  for (auto&[p, r] : branches) {
    p->bind(cm);
    r->bind(cm);
  }
  what->bind(cm);
}
ir::lang::var match_with::ir_compile(ir_sections_t) {
  THROW_UNIMPLEMENTED
}
free_vars_t let_in::free_vars() {
  return d->free_vars(e->free_vars());
}
capture_set let_in::capture_group() {
  return d->capture_group(e->capture_group());
}
void let_in::bind(const constr_map &cm) {
  d->bind(cm);
  e->bind(cm);
}
void let_in::compile(direct_sections_t s, size_t stack_pos) {
  size_t new_stack_pos = d->compile_locally(s, stack_pos);
  e->compile(s, new_stack_pos);
  if (new_stack_pos > stack_pos)
    s.main << "add rsp, " << 8 * (new_stack_pos - stack_pos) << " ; retrieving space of variables from let_in\n";
}
ir::lang::var let_in::ir_compile(ir_sections_t) {
  THROW_UNIMPLEMENTED
}
fun::fun(std::vector<matcher::ptr> &&args, ptr &&body) : args(std::move(args)), body(std::move(body)) {}
free_vars_t fun::free_vars() {
  auto fv = body->free_vars();
  std::string s = util::texp::make(fv)->to_string();
  for (auto &arg : args)arg->bind(fv);
  std::string s2 = util::texp::make(fv)->to_string();

  return std::move(fv);
}
capture_set fun::capture_group() {
  auto cs = body->capture_group();
  for (auto &arg : args)arg->bind(cs);
  captures.assign(cs.cbegin(), cs.cend());
  std::sort(captures.begin(), captures.end());
  return std::move(cs);
}
void fun::compile(direct_sections_t s, size_t stack_pos) {
  std::string name = compile_global(s);
  if (captures.empty()) {
    s.data << name << "__pure_fun_block" << " dq 1," << args.size() << "," << name
           << "; FN_BASE_PURE, n_args, text_ptr  \n" << std::endl;
    s.main << "mov rax, " << name << "__pure_fun_block\n";
  } else {
    s.main << "mov rdi, " << (4 + captures.size()) * 8 << "\n";
    s.main << "call malloc \n";
    s.main << "push r13;\n";
    ++stack_pos;
    s.main << "mov r13, rax\n";
    s.main << "mov qword [r13], 3; FN_BASE_CLOSURE \n";
    s.main << "mov qword [r13+8], " << args.size() << "; n_args \n";
    s.main << "mov qword [r13+16], " << name << "; text_ptr \n";
    s.main << "mov qword [r13+24], " << captures.size() << "; captures_size \n";
    for (size_t i = 0; i < captures.size(); ++i) {
      identifier mock_id(captures[i]->name);
      mock_id.definition_point = captures[i];
      mock_id.compile(s, stack_pos);
      s.main << "mov qword [r13+" << (i + 4) * 8 << "], rax ; captured " << captures[i]->name << "\n";
    }
    s.main << "mov rax, r13\n";
    s.main << "pop r13;\n";
    --stack_pos;
  }

}
void fun::bind(const constr_map &cm) {
  for (auto &arg : args)arg->bind(cm);
  body->bind(cm);
}
bool fun::is_capturing(const matcher::universal_matcher *m) const {
  return std::binary_search(captures.begin(), captures.end(), m);
}
size_t fun::capture_index(const matcher::universal_matcher *m) const {
  return std::distance(captures.begin(), std::lower_bound(captures.begin(), captures.end(), m));
}
std::string fun::text_name_gen(std::string_view name_hint) {
  if (name_hint.empty()) {
    static size_t id = 0;
    std::string s = ("___unnamed_fn");
    if (id)s.append("__").append(std::to_string(id));
    ++id;
    return s;
  } else {
    static size_t id = 0;
    std::string s = ("___");
    s.append(name_hint).append("_fn");
    if (id)s.append("__").append(std::to_string(id));
    ++id;
    return s;
  }
}

std::string fun::ir_compile_global(ir_sections_t s) {
  assert(this->captures.empty());
  static size_t fun_id_gen = 1;
  const size_t fun_id = fun_id_gen++;
  std::string name = std::string("__fun_").append(std::to_string(fun_id)).append("__");
  using namespace ir::lang;
  function f;
  var arg_block;
  f.args = {arg_block};
  f.name = name;
  //read unroll args onto variables
  bool should_skip = false;
  // iterate the args in reverse order
  for (auto arg_it = args.rbegin(); arg_it != args.rend(); ++arg_it) {
    if (should_skip)arg_block = f.declare_assign(arg_block[2]);
    should_skip = true;
    (*arg_it)->ir_locally_unroll(f, f.declare_assign(arg_block[4]));
  }
  //compute value
  f.ret = body->ir_compile(s.with_main(f));
  *(s.text++) = std::move(f);
  return name;
}

std::string fun::compile_global(direct_sections_t s, std::string_view name_hint) {
  const bool has_captures = !captures.empty(); // If it's global it should not be capturing anything

  std::string text_ptr = text_name_gen(name_hint);

  std::stringstream this_fun;
  size_t this_stack_pos = 0;
  this_fun << text_ptr << ":\n";
  this_fun << "push r12\n";
  ++this_stack_pos;
  this_fun << "mov r12, rdi\n";
  bool should_skip = false;
  // iterate the args in reverse order
  for (auto arg_it = args.rbegin(); arg_it != args.rend(); ++arg_it) {
    if (should_skip)this_fun << "mov r12, qword [r12+16]\n";
    should_skip = true;
    this_fun << "mov rax, qword [r12+24]\n";
    this_stack_pos = (*arg_it)->locally_unroll(this_fun, this_stack_pos);
  }
  if (has_captures)
    this_fun << "mov r12, qword [r12+16]\n"
             << "; assert qword[r12+24] == " << captures.size()
             << "; i.e. the right number of captures has been saved\n";

  body->compile(s.with_main(this_fun, this), this_stack_pos);

  if (this_stack_pos > 1) {
    this_fun << "add rsp, " << (8 * (this_stack_pos - 1)) << " ; popping fun args \n";
    this_stack_pos = 1;
  }

  this_fun << "pop r12\n";
  --this_stack_pos;
  this_fun << "ret\n";
  s.text << this_fun.str() << std::endl;
  return text_ptr;
}
ir::lang::var fun::ir_compile(ir_sections_t) {
  THROW_UNIMPLEMENTED
}

}

namespace definition {
bool def::is_tuple() const { return dynamic_cast<expression::build_tuple *>(e.get()); }
bool def::is_constr() const { return dynamic_cast<expression::constructor *>(e.get()); }
bool def::is_fun() const { return dynamic_cast<expression::fun *>(e.get()); }
bool def::is_single_name() const { return dynamic_cast<matcher::universal_matcher *>(name.get()); }
def::def(matcher::ptr &&name, expression::ptr e) : name(std::move(name)), e(std::move(e)) {}

free_vars_t t::free_vars(free_vars_t &&fv) {
  if (rec) {
    for (auto &def : defs)fv = join_free_vars(std::move(fv), def.e->free_vars());
    for (auto &def : defs)def.name->bind(fv);
  } else {
    for (auto &def : defs)def.name->bind(fv);
    for (auto &def : defs)fv = join_free_vars(std::move(fv), def.e->free_vars());
  }
  return fv;
}
capture_set t::capture_group(capture_set &&cs) {
  if (rec) {
    for (auto &def : defs)cs = join_capture_set(std::move(cs), def.e->capture_group());
    for (auto &def : defs)def.name->bind(cs);
  } else {
    for (auto &def : defs)def.name->bind(cs);
    for (auto &def : defs)cs = join_capture_set(std::move(cs), def.e->capture_group());
  }
  return cs;
}

void t::ir_compile_global(ir_sections_t s) {

  using namespace ir::lang;

  if (rec) {
    //check that all values are construcive w.r.t each other
    // 1. check no name clashes
    // 2. check that if one contains another
    THROW_UNIMPLEMENTED

  }
  for (auto &def : defs)
    if (def.is_single_name() && (def.is_fun() || def.is_tuple() || def.is_constr())) {
      dynamic_cast<matcher::universal_matcher *>(def.name.get())->use_as_immediate = true;
    }
  for (auto &def : defs) {
    matcher::universal_matcher *name = dynamic_cast<matcher::universal_matcher *>(def.name.get());
    if (name && def.is_fun()) {
      expression::fun *f = dynamic_cast<expression::fun *>(def.e.get());
      assert(f->captures.empty());
      std::string text_ptr = f->ir_compile_global(s);
      name->ir_allocate_globally_funblock(s.data,f->args.size(),text_ptr);
    } else if (name && def.is_tuple()) {

      expression::build_tuple *e = dynamic_cast<expression::build_tuple *>(def.e.get());
      name->ir_allocate_global_tuple(s.data, e->args.size());
      var tuple_addr = s.main.declare_global(name->ir_asm_name());
      assert(name->use_as_immediate);
      size_t i = 0;
      for (auto &e : e->args) {
        auto v = e->ir_compile(s);
        s.main.push_back(instruction::write_uninitialized_mem{.base = tuple_addr, .block_offset = i + 2, .src = v});
        ++i;
      }
    } else if (name && def.is_constr()) {

      expression::constructor *e = dynamic_cast<expression::constructor *>(def.e.get());
      if (e->arg) {
        name->ir_allocate_global_constrblock(s.data, *e->definition_point);
        assert(name->use_as_immediate);
        auto v = e->arg->ir_compile(s);
        var constr_addr = s.main.declare_global(name->ir_asm_name());
        s.main.push_back(instruction::write_uninitialized_mem{.base = constr_addr, .block_offset = 2, .src = v});
      } else {
        name->ir_allocate_global_constrimm(s.data, *e->definition_point);
        assert(!name->use_as_immediate);
      }

    } else {
      def.name->ir_allocate_global_value(s.data);
      def.name->ir_global_unroll(s.main, def.e->ir_compile(s)); // take rax, unroll it onto globals.
    }
  }
}
void t::compile_global(direct_sections_t s) {
  if (rec) {
    //check that all values are construcive w.r.t each other
    // 1. check no name clashes
    // 2. check that if one contains another
    //THROW_UNIMPLEMENTED

  }
  for (auto &def : defs)
    if (def.is_single_name() && (def.is_fun() || def.is_tuple() || def.is_constr())) {
      dynamic_cast<matcher::universal_matcher *>(def.name.get())->use_as_immediate = true;
    }
  for (auto &def : defs) {
    matcher::universal_matcher *name = dynamic_cast<matcher::universal_matcher *>(def.name.get());
    if (name && def.is_fun()) {
      expression::fun *f = dynamic_cast<expression::fun *>(def.e.get());
      assert(f->captures.empty());
      std::string text_ptr = f->compile_global(s, name->name);
      name->globally_allocate_funblock(f->args.size(), s.data, text_ptr);
    } else if (name && def.is_tuple()) {
      expression::build_tuple *e = dynamic_cast<expression::build_tuple *>(def.e.get());
      name->globally_allocate_tupleblock(s.data, e->args.size());
      assert(name->use_as_immediate);
      size_t i = 0;
      for (auto &e : e->args) {
        e->compile(s, 0);
        s.main << "mov qword[" << name->asm_name();
        if (i)s.main << "+" << (8 * i);
        s.main << "], rax\n";
        ++i;
      }
    } else if (name && def.is_constr()) {
      expression::constructor *e = dynamic_cast<expression::constructor *>(def.e.get());
      if (e->arg) {
        name->globally_allocate_constrblock(s.data, *e->definition_point);
        assert(name->use_as_immediate);
        e->arg->compile(s, 0);
        s.main << "mov qword[" << name->asm_name() << "+8], rax\n";
      } else {
        name->globally_allocate_constrimm(s.data, *e->definition_point);
        assert(!name->use_as_immediate);
      }
    } else {
      def.name->globally_allocate(s.data);
      def.e->compile(s, 0); // the value is left on rax
      def.name->global_unroll(s.main); // take rax, unroll it onto globals.
    }
  }

}
size_t t::compile_locally(direct_sections_t s, size_t stack_pos) {
  if (rec) {
    THROW_UNIMPLEMENTED
  } else {
    for (auto &def : defs) {
      def.e->compile(s, stack_pos); // put value in rax
      stack_pos = def.name->locally_unroll(s.main, stack_pos); // load value from rax
    }
    return stack_pos;
  }
  return 0;
}
void t::bind(const constr_map &cm) {
  for (auto &d : defs)d.name->bind(cm), d.e->bind(cm);
}

}

matcher::universal_matcher::universal_matcher(std::string_view n) : name(n), stack_relative_pos(-1), top_level(false) {}
void matcher::universal_matcher::bind(free_vars_t &fv) {
  if (auto it = fv.find(name); it != fv.end()) {
    usages = std::move(it->second);
    fv.erase(it);
    for (auto i : usages) i->definition_point = this;

  } else {
    //TODO-someday: warning that the name is unused
  }
}
void matcher::universal_matcher::bind(capture_set &cs) {
  cs.erase(this);
}
void matcher::universal_matcher::bind(const constr_map &cm) {}
std::string matcher::universal_matcher::asm_name() const {
  return name_resolution_id ? std::string(name).append("_").append(std::to_string(name_resolution_id)) : std::string(
      name);
}

std::string matcher::universal_matcher::ir_asm_name() const {
  return std::string("__global_value_").append(std::to_string(name_resolution_id)).append("__");
}
void matcher::universal_matcher::ir_globally_register(global_map &m) {
  static size_t start_id = 1;
  top_level = true;
  m[name] = this;
  name_resolution_id = ++start_id;
}
void matcher::universal_matcher::globally_register(global_map &m) {
  top_level = true;
  if (auto[it, b] = m.try_emplace(name, this); !b) {
    name_resolution_id = it->second->name_resolution_id + 1;
    it->second = this;
  }
}
void matcher::universal_matcher::globally_allocate(std::ostream &os) {
  use_as_immediate = false;
  os << asm_name() << " dq 0" << std::endl;
}
void matcher::universal_matcher::ir_allocate_global_value(std::ostream &os) {
  use_as_immediate = false;
  os << ir_asm_name() << " dq 0 ; " << name << " : value \n";
}
void matcher::universal_matcher::globally_allocate_funblock(size_t n_args,
                                                            std::ostream &os,
                                                            std::string_view text_ptr) {
  use_as_immediate = true;
  os << asm_name() << " dq 1," << n_args << "," << text_ptr << "\n" << std::endl;
}

void matcher::universal_matcher::globally_allocate_tupleblock(std::ostream &os, size_t tuple_size) {
  use_as_immediate = true;
  os << asm_name() << " dq ";
  bool comma = false;
  while (tuple_size--) {
    if (comma)os << ", ";
    comma = true;
    os << "0";
  };
  os << "\n";
}

#define Tag_Tuple  0
#define Tag_Fun 1
#define Tag_Arg 2
uintptr_t uint_to_v(uint64_t x) {
  return (uintptr_t) ((x << 1) | 1);
}
uint64_t make_tag_size_d(uint32_t tag, uint32_t size, uint8_t d) {
  return (((uint64_t) tag) << 32) | (((uint64_t) size) << 1) | (d & 1);
}

void matcher::universal_matcher::ir_allocate_globally_funblock(std::ostream &os, size_t n_args,
                                                               std::string_view text_ptr) {
  use_as_immediate = true;
  os << ir_asm_name() << " dq 0, " << make_tag_size_d(Tag_Fun, 2, 0) << "," << text_ptr<< "," << uint_to_v(n_args) << "; " << name
     << " : funblock\n";
}
void matcher::universal_matcher::ir_allocate_global_tuple(std::ostream &os, size_t tuple_size) {
  use_as_immediate = true;
  os << ir_asm_name() << " dq 0," << make_tag_size_d(Tag_Tuple, tuple_size, 0);
  for (int i = 0; i < tuple_size; ++i)os << ", 0";
  os << "; " << name << " : tuple[" << tuple_size << "]\n";
}

void matcher::universal_matcher::ir_allocate_global_constrblock(std::ostream &os,
                                                                const type::definition::single_variant::constr &constr) {
  use_as_immediate = true;
  assert(!constr.is_immediate());
  os << ir_asm_name() << " dq 0," << make_tag_size_d(constr.tag, 1, 0) << ", 0   ; " << name << " : " << constr.name
     << " = " << constr.tag << "\n";
}

void matcher::universal_matcher::globally_allocate_constrblock(std::ostream &os,
                                                               const type::definition::single_variant::constr &constr) {
  use_as_immediate = true;
  assert(!constr.is_immediate());
  os << asm_name() << " dq " << constr.tag << ", 0   ; `" << constr.name << "` = " << constr.tag << "\n";
}
void matcher::universal_matcher::ir_allocate_global_constrimm(std::ostream &os,
                                                              const type::definition::single_variant::constr &constr) {
  use_as_immediate = false;
  assert(constr.is_immediate());
  os << ir_asm_name() << " dq " << constr.tag << "; " << name << " : " << constr.name << " = " << constr.tag << "\n";
}
void matcher::universal_matcher::globally_allocate_constrimm(std::ostream &os,
                                                             const type::definition::single_variant::constr &constr) {
  use_as_immediate = false;
  assert(constr.is_immediate());
  os << asm_name() << " dq " << constr.tag << "; `" << constr.name << "` = " << constr.tag << "\n";
}

void matcher::universal_matcher::global_unroll(std::ostream &os) {
  os << "mov qword [" << asm_name() << "], rax" << std::endl;
}

void matcher::universal_matcher::ir_global_unroll(ir::scope &s, ir::lang::var what) {
  assert(!use_as_immediate);
  using namespace ir::lang;
  var where = s.declare_global(ir_asm_name());
  s.push_back(instruction::write_uninitialized_mem{.base = where, .block_offset = 0, .src = what});
}

size_t matcher::universal_matcher::locally_unroll(std::ostream &os, size_t stack_pos) {
  ++stack_pos;
  stack_relative_pos = stack_pos;
  os << "push rax  ; " << name << " is on position " << stack_relative_pos << " on the stack\n";
  return stack_pos;
}
size_t matcher::universal_matcher::test_locally_unroll(std::ostream &os,
                                                       size_t stack_pos,
                                                       size_t caller_stack_pos,
                                                       std::string_view on_fail) {
  ++stack_pos;
  stack_relative_pos = stack_pos;
  os << "mov qword[rsp-" << 8 * (stack_pos - caller_stack_pos) << "], rax ; " << name << " is on position "
     << stack_relative_pos << " on the stack\n";
  return stack_pos;
}
void matcher::universal_matcher::globally_evaluate(std::ostream &os) const {
  assert(top_level);
  if (use_as_immediate) {
    os << "mov rax, " << asm_name() << std::endl;
  } else {
    os << "mov rax, qword [" << asm_name() << "]" << std::endl;
  }
}

ir::lang::var matcher::universal_matcher::ir_evaluate_global(ir::lang::scope &s) const {
  assert(top_level);
  using namespace ir::lang;
  var gl = s.declare_global(ir_asm_name());
  if (use_as_immediate) {
    return gl;
  } else {
    return s.declare_assign(gl[0]);
  }
}
void matcher::universal_matcher::ir_locally_unroll(ir::scope &s, ir::lang::var v) {
  assert(!top_level);
  using namespace ir::lang;
  s.push_back(instruction::assign{.dst = ir_var, .src = v});
}

void matcher::constructor_matcher::bind(free_vars_t &fv) {
  if (arg)arg->bind(fv);
}
void matcher::constructor_matcher::bind(capture_set &cs) {
  if (arg)arg->bind(cs);
}
void matcher::constructor_matcher::globally_register(global_map &m) {
  if (arg)arg->globally_register(m);
}
void matcher::constructor_matcher::ir_globally_register(global_map &m) {
  if (arg)arg->ir_globally_register(m);
}
void matcher::constructor_matcher::bind(const constr_map &cm) {
  if (auto it = cm.find(cons); it == cm.end()) {
    throw ast::unbound_constructor(cons);
  } else {
    definition_point = it->second;
  }
  if (arg)arg->bind(cm);
}
void matcher::constructor_matcher::global_unroll(std::ostream &os) {
  assert(definition_point);
  assert(definition_point->is_immediate() == (arg == nullptr));
  if (arg) {
    os << "mov rax, qword [rax+8]\n";
    arg->global_unroll(os);
  }
}
void matcher::constructor_matcher::ir_global_unroll(ir::scope &s, ir::lang::var block) {
  assert(definition_point);
  assert(definition_point->is_immediate() == (arg == nullptr));
  using namespace ir::lang;
  if (arg) {
    arg->ir_global_unroll(s, s.declare_assign(block[2]));
  }
}

size_t matcher::constructor_matcher::locally_unroll(std::ostream &os, size_t stack_pos) {
  assert(definition_point);
  assert(definition_point->is_immediate() == (arg == nullptr));
  if (arg) {
    os << "mov rax, qword [rax+8]\n";
    return arg->locally_unroll(os, stack_pos);
  } else return stack_pos;
}

void matcher::constructor_matcher::ir_locally_unroll(ir::scope &s, ir::lang::var block) {
  assert(definition_point);
  assert(definition_point->is_immediate() == (arg == nullptr));
  if (arg) {
    return arg->ir_locally_unroll(s,s.declare_assign(block[2]));
  }
}

size_t matcher::constructor_matcher::test_locally_unroll(std::ostream &os,
                                                         size_t stack_pos,
                                                         size_t caller_stack_pos,
                                                         std::string_view on_fail) {
  assert(definition_point);
  assert(definition_point->is_immediate() == (arg == nullptr));
  if (arg) {
    os << "test al, 1\n"
          "jne " << on_fail << "\n";
    os << "cmp qword [rax], " << definition_point->tag << " ; Tag = " << definition_point->name << "\n";
    os << "jne " << on_fail << "\n";
    os << "mov rax, qword [rax+8]\n";
    return arg->test_locally_unroll(os, stack_pos, caller_stack_pos, on_fail);
  } else {
    os << "cmp rax, " << definition_point->tag << " ; Tag = " << definition_point->name << "\n";
    os << "jne " << on_fail << "\n";
    return stack_pos;
  }
}

void matcher::tuple_matcher::bind(free_vars_t &fv) {
  for (auto &p : args)p->bind(fv);
}
void matcher::tuple_matcher::bind(capture_set &cs) {
  for (auto &p : args)p->bind(cs);
}
void matcher::tuple_matcher::bind(const constr_map &cm) {
  for (auto &p : args)p->bind(cm);
}
void matcher::tuple_matcher::globally_allocate(std::ostream &os) {
  for (auto &p : args)p->globally_allocate(os);
}
void matcher::tuple_matcher::ir_allocate_global_value(std::ostream &os) {
  for (auto &p : args)p->ir_allocate_global_value(os);
}
void matcher::tuple_matcher::globally_register(global_map &m) {
  for (auto &p : args)p->globally_register(m);
}
void matcher::tuple_matcher::ir_globally_register(global_map &m) {
  for (auto &p : args)p->ir_globally_register(m);
}
void matcher::tuple_matcher::global_unroll(std::ostream &os) {
  os << "push r12\n"
        "mov r12, rax\n";
  for (int i = 0; i < args.size(); ++i) {
    os << "mov rax, qword [r12" << (i ? std::string("+").append(std::to_string(i * 8)) : "") << "]\n";
    args.at(i)->global_unroll(os);
  }
  os << "pop r12\n";
}
void matcher::tuple_matcher::ir_global_unroll(ir::scope &s, ir::lang::var block) {
  using namespace ir::lang;
  for (size_t i = 0; i < args.size(); ++i) {
    var content = s.declare_assign(block[i + 2]);
    args.at(i)->ir_global_unroll(s, content);
  }
}

void matcher::tuple_matcher::ir_locally_unroll(ir::scope &s, ir::lang::var block) {
  using namespace ir::lang;
  for (size_t i = 0; i < args.size(); ++i) {
    var content = s.declare_assign(block[i + 2]);
    args.at(i)->ir_locally_unroll(s, content);
  }
}

size_t matcher::tuple_matcher::locally_unroll(std::ostream &os, size_t stack_pos) {
  size_t rax_pos = stack_pos + stack_unrolling_dimension(); // The first free position in stack
  os << "mov qword [rsp-" << 8 * (rax_pos - stack_pos) << "], rax ; save rax\n";
  for (int i = 0; i < args.size(); ++i) {
    if (i)os << "mov rax, qword [rsp-" << 8 * (rax_pos - stack_pos) << "] ; restore rax\n";
    os << "mov rax, qword [rax" << (i ? std::string("+").append(std::to_string(i * 8)) : "") << "] "
       << "; tuple_matcher " << loc << " " << (i + 1) << " of " << args.size() << " \n";
    stack_pos = args.at(i)->locally_unroll(os, stack_pos);

  }
  return stack_pos;
}
size_t matcher::tuple_matcher::test_locally_unroll(std::ostream &os,
                                                   size_t stack_pos,
                                                   size_t caller_stack_pos,
                                                   std::string_view on_fail) {

  size_t rax_pos = stack_pos + stack_unrolling_dimension(); // The first free position in stack
  os << "mov qword [rsp-" << 8 * (rax_pos - caller_stack_pos) << "], rax ; save rax\n";
  for (int i = 0; i < args.size(); ++i) {
    if (i)os << "mov rax, qword [rsp-" << 8 * (rax_pos - caller_stack_pos) << "] ; restore rax\n";
    os << "mov rax, qword [rax" << (i ? std::string("+").append(std::to_string(i * 8)) : "") << "] "
       << "; tuple_matcher " << (i + 1) << " of " << args.size() << " \n";
    stack_pos = args.at(i)->test_locally_unroll(os, stack_pos, caller_stack_pos, on_fail);

  }
  return stack_pos;
}
size_t matcher::tuple_matcher::unrolled_size() const {
  size_t s = 0;
  for (auto &m : args)s += m->unrolled_size();
  return s;
}
size_t matcher::tuple_matcher::stack_unrolling_dimension() const {
  size_t s = 0, d = 0;
  for (auto &m : args) {
    d = std::max(d, s + m->stack_unrolling_dimension());
    s += m->unrolled_size();
  }
  ++d;
  return d;
}

namespace literal {
uint64_t integer::to_value() const {
  return uint64_t((value << 1) | 1);
}
uint64_t boolean::to_value() const { return value ? 3 : 1; }
uint64_t string::to_value() const { THROW_UNIMPLEMENTED; }
}
size_t matcher::literal_matcher::test_locally_unroll(std::ostream &os,
                                                     size_t stack_pos,
                                                     size_t caller_stack_pos,
                                                     std::string_view on_fail) {
  os << "cmp rax, " << value->to_value() << " ; literal " << loc << "\n";
  os << "jne " << on_fail << " \n";
  return stack_pos;
}
}
