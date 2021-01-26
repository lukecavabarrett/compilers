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

#define Tag_Tuple  0
#define Tag_Fun 1
#define Tag_Arg 2
#define Tag_String 3

uintptr_t uint_to_v(uint64_t x) {
  return (uintptr_t) ((x << 1) | 1);
}
uint64_t make_tag_size_d(uint32_t tag, uint32_t size, uint8_t d) {
  return (((uint64_t) tag) << 32) | (((uint64_t) size) << 1) | (d & 1);
}

}

namespace expression {

void identifier::bind(const constr_map &) {}
void fun_app::bind(const constr_map &cm) {
  f->bind(cm);
  x->bind(cm);
}
void destroy::bind(const constr_map &cm) {
  obj->bind(cm);
  d->bind(cm);
}
void if_then_else::bind(const constr_map &cm) {
  condition->bind(cm);
  true_branch->bind(cm);
  false_branch->bind(cm);
}
void constructor::bind(const constr_map &cm) {
  if (auto it = cm.find(name); it == cm.end()) {
    throw ast::unbound_constructor(name);
  } else {
    definition_point = it->second;
  }
  if (arg)arg->bind(cm);
}
void seq::bind(const constr_map &cm) {
  a->bind(cm);
  b->bind(cm);
}
void match_with::bind(const constr_map &cm) {
  for (auto&[p, r] : branches) {
    p->bind(cm);
    r->bind(cm);
  }
  what->bind(cm);
}
void tuple::bind(const constr_map &cm) {
  for (auto &p : args)p->bind(cm);
}
void let_in::bind(const constr_map &cm) {
  d->bind(cm);
  e->bind(cm);
}
void fun::bind(const constr_map &cm) {
  for (auto &arg : args)arg->bind(cm);
  body->bind(cm);
}

free_vars_t literal::free_vars() { return {}; }
free_vars_t identifier::free_vars() {
  std::string n(name);
  if (definition_point && definition_point->top_level) return {};
  return {{name, {this}}};
}
free_vars_t tuple::free_vars() {
  free_vars_t fv;
  for (auto &p : args)fv = join_free_vars(p->free_vars(), std::move(fv));
  return fv;
}
free_vars_t fun_app::free_vars() { return join_free_vars(f->free_vars(), x->free_vars()); }
free_vars_t destroy::free_vars() { return join_free_vars(obj->free_vars(), d->free_vars()); }
free_vars_t constructor::free_vars() {
  if (arg)return arg->free_vars();
  return {};
}
free_vars_t if_then_else::free_vars() {
  return join_free_vars(join_free_vars(condition->free_vars(),
                                       true_branch->free_vars()),
                        false_branch->free_vars());
}
free_vars_t seq::free_vars() { return join_free_vars(a->free_vars(), b->free_vars()); }
free_vars_t match_with::free_vars() {
  free_vars_t fv = what->free_vars();
  for (auto&[p, r] : branches) {
    free_vars_t bfv = r->free_vars();
    p->bind(bfv);
    fv = join_free_vars(std::move(fv), std::move(bfv));
  }
  return fv;
}
free_vars_t let_in::free_vars() {
  return d->free_vars(e->free_vars());
}
free_vars_t fun::free_vars() {
  auto fv = body->free_vars();
  for (auto &arg : args)arg->bind(fv);

  return std::move(fv);
}

capture_set identifier::capture_group() {
  if (!definition_point) {
    std::string_view n = name;
  }
  if (!definition_point)THROW_INTERNAL_ERROR;
  if (definition_point->top_level) return {};
  return {definition_point};
}
capture_set literal::capture_group() { return {}; }
capture_set constructor::capture_group() {
  if (arg)arg->capture_group();
  return {};
}
capture_set if_then_else::capture_group() {
  return join_capture_set(join_capture_set(condition->capture_group(),
                                           true_branch->capture_group()),
                          false_branch->capture_group());
}
capture_set tuple::capture_group() {
  capture_set fv;
  for (auto &p : args)fv = join_capture_set(p->capture_group(), std::move(fv));
  return fv;
}
capture_set fun_app::capture_group() { return join_capture_set(f->capture_group(), x->capture_group()); }
capture_set destroy::capture_group() { return join_capture_set(obj->capture_group(), d->capture_group());; }
capture_set seq::capture_group() { return join_capture_set(a->capture_group(), b->capture_group()); }
capture_set match_with::capture_group() {
  capture_set cs = what->capture_group();
  for (auto&[p, r] : branches) {
    capture_set bcs = r->capture_group();
    p->bind(bcs);
    cs = join_capture_set(std::move(cs), std::move(bcs));
  }
  return cs;
}
capture_set let_in::capture_group() {
  return d->capture_group(e->capture_group());
}
capture_set fun::capture_group() {
  auto cs = body->capture_group();
  for (auto &arg : args)arg->bind(cs);
  captures.assign(cs.cbegin(), cs.cend());
  std::sort(captures.begin(), captures.end());
  return std::move(cs);
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
void literal::compile(direct_sections_t s, size_t stack_pos) {
  s.main << "mov rax, " << std::to_string(value->to_value()) << std::endl;
}
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
void tuple::compile(direct_sections_t s, size_t stack_pos) {
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
void destroy::compile(direct_sections_t s, size_t stack_pos) { THROW_UNIMPLEMENTED; }
void seq::compile(direct_sections_t s, size_t stack_pos) {
  a->compile(s, stack_pos);
  b->compile(s, stack_pos);
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
void let_in::compile(direct_sections_t s, size_t stack_pos) {
  size_t new_stack_pos = d->compile_locally(s, stack_pos);
  e->compile(s, new_stack_pos);
  if (new_stack_pos > stack_pos)
    s.main << "add rsp, " << 8 * (new_stack_pos - stack_pos) << " ; retrieving space of variables from let_in\n";
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

ir::lang::var identifier::ir_compile(ir_sections_t s) {
  if (definition_point->top_level) {
    return definition_point->ir_evaluate_global(s.main);
  } else {
    return definition_point->ir_var;
  }
}
ir::lang::var literal::ir_compile(ir_sections_t s) {
  return value->ir_compile(s);
}
ir::lang::var constructor::ir_compile(ir_sections_t s) {
  using namespace ir::lang;
  assert(definition_point->tag >= 5);
  assert(definition_point->tag & 1);
  if (arg) {
    var content = arg->ir_compile(s);
    var block = s.main.declare_assign(rhs_expr::malloc{.size=3});
    s.main.push_back(instruction::write_uninitialized_mem{.base = block, .block_offset = 0, .src = s.main.declare_constant(
        3)});
    s.main.push_back(instruction::write_uninitialized_mem{.base = block, .block_offset = 1, .src = s.main.declare_constant(
        make_tag_size_d(definition_point->tag, 1, 0))});
    s.main.push_back(instruction::write_uninitialized_mem{.base = block, .block_offset = 2, .src = content});
    return block;
  } else {
    assert(definition_point->tag & 1);
    return s.main.declare_constant(definition_point->tag);
  }
}
ir::lang::var constructor::ir_compile_with_destructor(ir_sections_t s, ir::lang::var d) {
  using namespace ir::lang;
  assert(definition_point->tag >= 5);
  assert(definition_point->tag & 1);
  assert(arg);
  var content = arg->ir_compile(s);
  var block = s.main.declare_assign(rhs_expr::malloc{.size=4});
  s.main.push_back(instruction::write_uninitialized_mem{.base = block, .block_offset = 0, .src = s.main.declare_constant(
      3)});
  s.main.push_back(instruction::write_uninitialized_mem{.base = block, .block_offset = 1, .src = s.main.declare_constant(
      make_tag_size_d(definition_point->tag, 1, 1))});
  s.main.push_back(instruction::write_uninitialized_mem{.base = block, .block_offset = 2, .src = content});
  s.main.push_back(instruction::write_uninitialized_mem{.base = block, .block_offset = 3, .src = d});
  return block;
}
ir::lang::var if_then_else::ir_compile(ir_sections_t s) {
  using namespace ir::lang;
  s.main.push_back(instruction::cmp_vars{.v1 = condition->ir_compile(s), .v2 = s.main.declare_constant(2), .op=instruction::cmp_vars::test});
  scope true_scope, false_scope;
  true_scope.ret = true_branch->ir_compile(s.with_main(true_scope));
  false_scope.ret = false_branch->ir_compile(s.with_main(false_scope));
  return s.main.declare_assign(std::make_unique<ternary>(ternary{.cond = ternary::jmp_instr::jz, .nojmp_branch = std::move(
      true_scope), .jmp_branch = std::move(false_scope)}));
}
ir::lang::var tuple::ir_compile(ir_sections_t s) {
  using namespace ir::lang;
  var block = s.main.declare_assign(rhs_expr::malloc{.size=2 + args.size()});
  s.main.push_back(instruction::write_uninitialized_mem{.base=block, .block_offset=0, .src=s.main.declare_constant(3)});
  s.main.push_back(instruction::write_uninitialized_mem{.base=block, .block_offset=1, .src=s.main.declare_constant(
      make_tag_size_d(Tag_Tuple, args.size(), 0))});
  for (size_t i = 0; i < args.size(); ++i) {
    s.main.push_back(instruction::write_uninitialized_mem{.base=block, .block_offset=2 + i, .src=args.at(i)->ir_compile(
        s)});
  }
  return block;
}
ir::lang::var tuple::ir_compile_with_destructor(ir_sections_t s, ir::lang::var d) {
  using namespace ir::lang;
  var block = s.main.declare_assign(rhs_expr::malloc{.size=2 + args.size() + 1});
  s.main.push_back(instruction::write_uninitialized_mem{.base=block, .block_offset=0, .src=s.main.declare_constant(3)});
  s.main.push_back(instruction::write_uninitialized_mem{.base=block, .block_offset=1, .src=s.main.declare_constant(
      make_tag_size_d(Tag_Tuple, args.size(), 1))});
  for (size_t i = 0; i < args.size(); ++i) {
    s.main.push_back(instruction::write_uninitialized_mem{.base=block, .block_offset=2 + i, .src=args.at(i)->ir_compile(
        s)});
  }
  s.main.push_back(instruction::write_uninitialized_mem{.base=block, .block_offset=2 + args.size(), .src=d});
  return block;
}
ir::lang::var fun_app::ir_compile(ir_sections_t s) {
  using namespace ir::lang;

  //optimize for arithmetic
  if (auto *unary_f = dynamic_cast<identifier *>(this->f.get()); unary_f && unary_f->name.starts_with("__unary_op__")) {
    //unary
    if (unary_f->name == "__unary_op__PLUS__")return x->ir_compile(s).mark(trivial);
    if (unary_f->name == "__unary_op__MINUS__")
      return s.main.declare_assign(rhs_expr::unary_op{
          .op=rhs_expr::unary_op::neg, .x =  x->ir_compile(s).mark(trivial)}).mark(trivial);
    THROW_INTERNAL_ERROR // no other unary operators
  }

  //TODO: support all arithmetic
  if (auto *fa = dynamic_cast<fun_app *>(f.get()); fa)
    if (auto *binary_f = dynamic_cast<identifier *>(fa->f.get()); binary_f
        && util::is_in<std::string_view>(binary_f->name,
                                         {"__binary_op__PLUS__", "__binary_op__MINUS__", "__binary_op__STAR__"})) {
      //binary op
      //TODO-someday: addition and subtraction might require a single rather than three ops
      var a_v = fa->x->ir_compile(s).mark(trivial);
      var a = s.main.declare_assign(rhs_expr::unary_op{.op = rhs_expr::unary_op::v_to_int, .x = a_v}).mark(trivial);

      var b_v = x->ir_compile(s).mark(trivial);
      var b = s.main.declare_assign(rhs_expr::unary_op{.op = rhs_expr::unary_op::v_to_int, .x = b_v}).mark(trivial);

      auto with_unary = [&](rhs_expr::binary_op::ops o) {
        var a_op_b = s.main.declare_assign(rhs_expr::binary_op{.op = o, .x1 = a, .x2 = b}).mark(trivial);
        var a_op_b_v =
            s.main.declare_assign(rhs_expr::unary_op{.op = rhs_expr::unary_op::int_to_v, .x = a_op_b}).mark(trivial);
        return a_op_b_v;
      };
      if (binary_f->name == "__binary_op__PLUS__")return with_unary(rhs_expr::binary_op::add);
      if (binary_f->name == "__binary_op__MINUS__")return with_unary(rhs_expr::binary_op::sub);
      if (binary_f->name == "__binary_op__STAR__")return with_unary(rhs_expr::binary_op::imul);
      //TODO: add others
      THROW_INTERNAL_ERROR

    }

  //trivial case, a normal function
  var vf = f->ir_compile(s);
  var vx = x->ir_compile(s);
  return s.main.declare_assign(rhs_expr::apply_fn{.f = vf, .x = vx});
}
ir::lang::var destroy::ir_compile(ir_sections_t s) {
  if (auto *t = dynamic_cast<tuple *>(  obj.get());t) {
    return t->ir_compile_with_destructor(s, d->ir_compile(s));
  }
  if (auto *c = dynamic_cast<constructor *>(  obj.get());c) {
    return c->ir_compile_with_destructor(s, d->ir_compile(s));
  }
  THROW_INTERNAL_ERROR; //cannot have destructor on a non boxed
}
ir::lang::var seq::ir_compile(ir_sections_t s) {
  a->ir_compile(s);
  return b->ir_compile(s);
}
ir::lang::var match_with::ir_compile(ir_sections_t s) {
  assert(!branches.empty());
  using namespace ir::lang;
  var to_match = what->ir_compile(s);

  scope *current = &s.main;

  std::optional<var> returned;
  for (const auto &branch : branches) {
    branch.pattern->print(current->comment() << "matching ");
    var cond = branch.pattern->ir_test_unroll(*current, to_match).mark(trivial);
    current->push_back(instruction::cmp_vars{.v1 = cond, .v2 = current->declare_constant(2), .op=instruction::cmp_vars::test});
    auto if_applies = std::make_unique<ternary>();
    if_applies->cond = ternary::jz;
    //if_applies->nojmp_branch; // if applies
    //if_applies->jmp_branch; // if not applies
    scope &branch_scope = if_applies->nojmp_branch;
    branch.pattern->ir_locally_unroll(branch_scope, to_match);
    branch_scope.ret = branch.result->ir_compile(s.with_main(branch_scope));

    scope *on_fail = &if_applies->jmp_branch;
    auto v = current->declare_assign(std::move(if_applies));
    if (returned.has_value())current->ret = v; else returned.emplace(v);
    current = on_fail;
  }
  current->ret =
      current->declare_assign(rhs_expr::apply_fn{.f=current->declare_global("__throw__unmatched__"), .x=current->declare_constant(
          3)});
  return returned.value();
}
ir::lang::var let_in::ir_compile(ir_sections_t s) {
  d->ir_compile_locally(s);
  return e->ir_compile(s);
}
ir::lang::var fun::ir_compile(ir_sections_t s) {
  std::string text_ptr = ir_compile_global(s);
  if (captures.empty()) {
    static size_t id = 1;
    std::string name = "__pure_fun_block_";
    name.append(std::to_string(id++)).append("__");
    s.data << name << " dq 0," << make_tag_size_d(Tag_Fun, 2, 0) << "," << text_ptr << "," << uint_to_v(args.size())
           << "\n";
    return s.main.declare_global(name);

  } else {

    static size_t id = 1;
    using namespace ir::lang;
    var block = s.main.declare_assign(rhs_expr::malloc{.size=4 + captures.size()});
    s.main.push_back(instruction::write_uninitialized_mem{.base=block, .block_offset=0, .src=s.main.declare_constant(3)});
    s.main.push_back(instruction::write_uninitialized_mem{.base=block, .block_offset=1, .src=s.main.declare_constant(
        make_tag_size_d(Tag_Fun, 2 + captures.size(), 0))});
    s.main.push_back(instruction::write_uninitialized_mem{.base=block, .block_offset=2, .src=s.main.declare_global(
        text_ptr)});
    s.main.push_back(instruction::write_uninitialized_mem{.base=block, .block_offset=3, .src=s.main.declare_constant(
        uint_to_v(args.size()))});
    for (size_t i = 0; i < captures.size(); ++i) {
      s.main.push_back(instruction::write_uninitialized_mem{.base=block, .block_offset=4
          + i, .src=captures.at(i)->ir_var});
    }
    return block;
  }
}
std::string fun::ir_compile_global(ir_sections_t s) {
  const bool has_captures = !captures.empty(); // If it's global it should not be capturing anything
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
  if (has_captures) {
    arg_block = f.declare_assign(arg_block[2]);
    size_t id = 4;
    for (auto &c : captures) {
      f.push_back(instruction::assign{.dst= c->ir_var, .src=arg_block[id]});
      ++id;
    }
  }
  //compute value
  f.ret = body->ir_compile(s.with_main(f));
  *(s.text++) = std::move(f);
  return name;
}

identifier::identifier(std::string_view n) : t(n), name(n), definition_point(nullptr) {}
identifier::identifier(std::string_view n, std::string_view loc) : t(loc), name(n), definition_point(nullptr) {}

literal::literal(ast::literal::ptr &&v) : value(std::move(v)) {}

constructor::constructor(std::string_view n) : name(n) {}

if_then_else::if_then_else(ptr &&condition, ptr &&true_branch, ptr &&false_branch)
    : condition(std::move(condition)), true_branch(std::move(true_branch)), false_branch(std::move(false_branch)) {}

tuple::tuple(std::vector<expression::ptr> &&args) : args(std::move(args)) {}

fun_app::fun_app(ptr &&f_, ptr &&x_) : f(std::move(f_)), x(std::move(x_)) {
  loc = unite_sv(f, x);
}

destroy::destroy(ptr &&obj_, ptr &&d_) : obj(std::move(obj_)), d(std::move(d_)) {
  loc = unite_sv(obj, d);
}

seq::seq(ptr &&a, ptr &&b) : a(std::move(a)), b(std::move(b)) { loc = unite_sv(this->a, this->b); }

match_with::match_with(expression::ptr &&w) : what(std::move(w)) {}

fun::fun(std::vector<matcher::ptr> &&args, ptr &&body) : args(std::move(args)), body(std::move(body)) {}
bool fun::is_capturing(const matcher::universal *m) const {
  return std::binary_search(captures.begin(), captures.end(), m);
}
size_t fun::capture_index(const matcher::universal *m) const {
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

}

namespace definition {
bool def::is_tuple() const { return dynamic_cast<expression::tuple *>(e.get()); }
bool def::is_constr() const { return dynamic_cast<expression::constructor *>(e.get()); }
bool def::is_fun() const { return dynamic_cast<expression::fun *>(e.get()); }
bool def::is_single_name() const { return dynamic_cast<matcher::universal *>(name.get()); }
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
void t::ir_compile_locally(ir_sections_t s) {
  if (rec) {
    //check that all values are construcive w.r.t each other
    // 1. check no name clashes
    // 2. check that if one contains another
    THROW_UNIMPLEMENTED
  }
  for (auto &def : defs) def.name->ir_locally_unroll(s.main, def.e->ir_compile(s));

}
void t::ir_compile_global(ir_sections_t s) {

  using namespace ir::lang;

  if (rec) {
    //TODO:
    // - check that all values are construcive w.r.t each other
    //     1. check no name clashes
    //     2. check that if one contains another
  }
  for (auto &def : defs)
    if (def.is_single_name() && (def.is_fun() || def.is_tuple() || def.is_constr())) {
      dynamic_cast<matcher::universal *>(def.name.get())->use_as_immediate = true;
    }
  for (auto &def : defs) {
    matcher::universal *name = dynamic_cast<matcher::universal *>(def.name.get());
    if (name && def.is_fun()) {
      expression::fun *f = dynamic_cast<expression::fun *>(def.e.get());
      assert(f->captures.empty());
      std::string text_ptr = f->ir_compile_global(s);
      name->ir_allocate_globally_funblock(s.data, f->args.size(), text_ptr);
    } else if (name && def.is_tuple()) {

      expression::tuple *e = dynamic_cast<expression::tuple *>(def.e.get());
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
      dynamic_cast<matcher::universal *>(def.name.get())->use_as_immediate = true;
    }
  for (auto &def : defs) {
    matcher::universal *name = dynamic_cast<matcher::universal *>(def.name.get());
    if (name && def.is_fun()) {
      expression::fun *f = dynamic_cast<expression::fun *>(def.e.get());
      assert(f->captures.empty());
      std::string text_ptr = f->compile_global(s, name->name);
      name->globally_allocate_funblock(f->args.size(), s.data, text_ptr);
    } else if (name && def.is_tuple()) {
      expression::tuple *e = dynamic_cast<expression::tuple *>(def.e.get());
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

namespace matcher {
//bind constructor map
void universal::bind(const constr_map &cm) {}
void constructor::bind(const constr_map &cm) {
  if (auto it = cm.find(cons); it == cm.end()) {
    throw ast::unbound_constructor(cons);
  } else {
    definition_point = it->second;
  }
  if (arg)arg->bind(cm);
}
void tuple::bind(const constr_map &cm) {
  for (auto &p : args)p->bind(cm);
}
void ignore::bind(capture_set &cs) {}

//bind free variables
void universal::bind(free_vars_t &fv) {
  if (auto it = fv.find(name); it != fv.end()) {
    usages = std::move(it->second);
    fv.erase(it);
    for (auto i : usages) i->definition_point = this;

  } else {
    //TODO-someday: warning that the name is unused
  }
}
void constructor::bind(free_vars_t &fv) {
  if (arg)arg->bind(fv);
}
void tuple::bind(free_vars_t &fv) {
  for (auto &p : args)p->bind(fv);
}
void ignore::bind(free_vars_t &fv) {}

//bind capture set
void universal::bind(capture_set &cs) {
  cs.erase(this);
}
void constructor::bind(capture_set &cs) {
  if (arg)arg->bind(cs);
}
void tuple::bind(capture_set &cs) {
  for (auto &p : args)p->bind(cs);
}
void ignore::bind(const constr_map &cm) {}

//ir_globally_register
void universal::ir_globally_register(global_map &m) {
  static size_t start_id = 1;
  top_level = true;
  m[name] = this;
  name_resolution_id = ++start_id;
}
void constructor::ir_globally_register(global_map &m) {
  if (arg)arg->ir_globally_register(m);
}
void tuple::ir_globally_register(global_map &m) {
  for (auto &p : args)p->ir_globally_register(m);
}
void ignore::ir_globally_register(global_map &m) {}

//ir_allocate_global_value
void universal::ir_allocate_global_value(std::ostream &os) {
  use_as_immediate = false;
  os << ir_asm_name() << " dq 0 ; " << name << " : value \n";
}
void constructor::ir_allocate_global_value(std::ostream &os) { if (arg)arg->ir_allocate_global_value(os); }
void tuple::ir_allocate_global_value(std::ostream &os) {
  for (auto &p : args)p->ir_allocate_global_value(os);
}
void ignore::ir_allocate_global_value(std::ostream &os) {}

//ir_global_unroll
void universal::ir_global_unroll(ir::scope &s, ir::lang::var what) {
  assert(!use_as_immediate);
  using namespace ir::lang;
  var where = s.declare_global(ir_asm_name());
  s.push_back(instruction::write_uninitialized_mem{.base = where, .block_offset = 0, .src = what});
}
void constructor::ir_global_unroll(ir::scope &s, ir::lang::var block) {
  assert(definition_point);
  assert(definition_point->is_immediate() == (arg == nullptr));
  using namespace ir::lang;
  if (arg) {
    arg->ir_global_unroll(s, s.declare_assign(block[2]));
  }
}
void tuple::ir_global_unroll(ir::scope &s, ir::lang::var block) {
  using namespace ir::lang;
  for (size_t i = 0; i < args.size(); ++i) {
    var content = s.declare_assign(block[i + 2]);
    args.at(i)->ir_global_unroll(s, content);
  }
}

//singles
std::string universal::asm_name() const {
  return name_resolution_id ? std::string(name).append("_").append(std::to_string(name_resolution_id)) : std::string(
      name);
}
universal::universal(std::string_view n) : name(n), stack_relative_pos(-1), top_level(false) {}

std::string universal::ir_asm_name() const {
  return std::string("__global_value_").append(std::to_string(name_resolution_id)).append("__");
}
void universal::globally_register(global_map &m) {
  top_level = true;
  if (auto[it, b] = m.try_emplace(name, this); !b) {
    name_resolution_id = it->second->name_resolution_id + 1;
    it->second = this;
  }
}
void universal::globally_allocate(std::ostream &os) {
  use_as_immediate = false;
  os << asm_name() << " dq 0" << std::endl;
}
void universal::globally_allocate_funblock(size_t n_args,
                                           std::ostream &os,
                                           std::string_view text_ptr) {
  use_as_immediate = true;
  os << asm_name() << " dq 1," << n_args << "," << text_ptr << "\n" << std::endl;
}

void universal::globally_allocate_tupleblock(std::ostream &os, size_t tuple_size) {
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

void universal::ir_allocate_globally_funblock(std::ostream &os, size_t n_args,
                                              std::string_view text_ptr) {
  use_as_immediate = true;
  os << ir_asm_name() << " dq 0, " << make_tag_size_d(Tag_Fun, 2, 0) << "," << text_ptr << "," << uint_to_v(n_args)
     << "; " << name
     << " : funblock\n";
}
void universal::ir_allocate_global_tuple(std::ostream &os, size_t tuple_size) {
  use_as_immediate = true;
  os << ir_asm_name() << " dq 0," << make_tag_size_d(Tag_Tuple, tuple_size, 0);
  for (int i = 0; i < tuple_size; ++i)os << ", 0";
  os << "; " << name << " : tuple[" << tuple_size << "]\n";
}

void universal::ir_allocate_global_constrblock(std::ostream &os,
                                               const type::definition::single_variant::constr &constr) {
  use_as_immediate = true;
  assert(!constr.is_immediate());
  os << ir_asm_name() << " dq 0," << make_tag_size_d(constr.tag, 1, 0) << ", 0   ; " << name << " : " << constr.name
     << " = " << constr.tag << "\n";
}

void universal::globally_allocate_constrblock(std::ostream &os,
                                              const type::definition::single_variant::constr &constr) {
  use_as_immediate = true;
  assert(!constr.is_immediate());
  os << asm_name() << " dq " << constr.tag << ", 0   ; `" << constr.name << "` = " << constr.tag << "\n";
}
void universal::ir_allocate_global_constrimm(std::ostream &os,
                                             const type::definition::single_variant::constr &constr) {
  use_as_immediate = false;
  assert(constr.is_immediate());
  os << ir_asm_name() << " dq " << constr.tag << "; " << name << " : " << constr.name << " = " << constr.tag << "\n";
}
void universal::globally_allocate_constrimm(std::ostream &os,
                                            const type::definition::single_variant::constr &constr) {
  use_as_immediate = false;
  assert(constr.is_immediate());
  os << asm_name() << " dq " << constr.tag << "; `" << constr.name << "` = " << constr.tag << "\n";
}

void universal::global_unroll(std::ostream &os) {
  os << "mov qword [" << asm_name() << "], rax" << std::endl;
}

size_t universal::locally_unroll(std::ostream &os, size_t stack_pos) {
  ++stack_pos;
  stack_relative_pos = stack_pos;
  os << "push rax  ; " << name << " is on position " << stack_relative_pos << " on the stack\n";
  return stack_pos;
}
size_t universal::test_locally_unroll(std::ostream &os,
                                      size_t stack_pos,
                                      size_t caller_stack_pos,
                                      std::string_view on_fail) {
  ++stack_pos;
  stack_relative_pos = stack_pos;
  os << "mov qword[rsp-" << 8 * (stack_pos - caller_stack_pos) << "], rax ; " << name << " is on position "
     << stack_relative_pos << " on the stack\n";
  return stack_pos;
}
void universal::globally_evaluate(std::ostream &os) const {
  assert(top_level);
  if (use_as_immediate) {
    os << "mov rax, " << asm_name() << std::endl;
  } else {
    os << "mov rax, qword [" << asm_name() << "]" << std::endl;
  }
}

ir::lang::var universal::ir_evaluate_global(ir::lang::scope &s) const {
  assert(top_level);
  using namespace ir::lang;
  var gl = s.declare_global(ir_asm_name());
  if (use_as_immediate) {
    return gl;
  } else {
    return s.declare_assign(gl[0]);
  }
}
void universal::ir_locally_unroll(ir::scope &s, ir::lang::var v) {
  assert(!top_level);
  using namespace ir::lang;
  s.push_back(instruction::assign{.dst = ir_var, .src = v});
  s.comment() << "local variable \"" << name << "\" is on " << ir_var;
}

void constructor::globally_register(global_map &m) {
  if (arg)arg->globally_register(m);
}
void constructor::global_unroll(std::ostream &os) {
  assert(definition_point);
  assert(definition_point->is_immediate() == (arg == nullptr));
  if (arg) {
    os << "mov rax, qword [rax+8]\n";
    arg->global_unroll(os);
  }
}

size_t constructor::locally_unroll(std::ostream &os, size_t stack_pos) {
  assert(definition_point);
  assert(definition_point->is_immediate() == (arg == nullptr));
  if (arg) {
    os << "mov rax, qword [rax+8]\n";
    return arg->locally_unroll(os, stack_pos);
  } else return stack_pos;
}

void constructor::ir_locally_unroll(ir::scope &s, ir::lang::var block) {
  assert(definition_point);
  assert(definition_point->is_immediate() == (arg == nullptr));
  if (arg) {
    return arg->ir_locally_unroll(s, s.declare_assign(block[2]));
  }
}

size_t constructor::test_locally_unroll(std::ostream &os,
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
ir::lang::var constructor::ir_test_unroll(ir::scope &s, ir::lang::var v) {
  using namespace ir::lang;
  assert(definition_point);
  assert(definition_point->is_immediate() == (arg == nullptr));
  if (arg) {
    s.push_back(instruction::cmp_vars{.v1=v, .v2=s.declare_constant(1), .op=instruction::cmp_vars::test});

    scope matching_tag, sfalse;
    matching_tag.ret = arg->ir_test_unroll(matching_tag, matching_tag.declare_assign(v[2]).mark(trivial));
    sfalse.ret = sfalse.declare_assign(1);
    using binary_op = rhs_expr::binary_op;
    scope sboxed, simmediate;
    sboxed.push_back(instruction::cmp_vars{.v1=sboxed.declare_assign(binary_op{.op=binary_op::sar, .x1=sboxed.declare_assign(
        v[1]).mark(trivial), .x2=sboxed.declare_constant(32)}).mark(trivial), .v2=sboxed.declare_constant(
        definition_point->tag), .op=instruction::cmp_vars::cmp});
    sboxed.ret =
        sboxed.declare_assign(std::make_unique<ternary>(ternary{.cond = ternary::jne, .nojmp_branch=std::move(
            matching_tag), .jmp_branch=std::move(
            sfalse)}));

    simmediate.ret = simmediate.declare_assign(1);
    return s.declare_assign(std::make_unique<ternary>(ternary{.cond = ternary::jnz, .nojmp_branch=std::move(sboxed), .jmp_branch=std::move(
        simmediate)}));
  } else {
    s.push_back(instruction::cmp_vars{.v1=v, .v2=s.declare_constant(definition_point->tag), .op=instruction::cmp_vars::cmp});
    scope strue, sfalse;
    strue.ret = strue.declare_assign(3);
    sfalse.ret = sfalse.declare_assign(1);
    return s.declare_assign(std::make_unique<ternary>(ternary{.cond = ternary::jne, .nojmp_branch=std::move(strue), .jmp_branch=std::move(
        sfalse)}));
  }
}

void tuple::globally_allocate(std::ostream &os) {
  for (auto &p : args)p->globally_allocate(os);
}
void tuple::globally_register(global_map &m) {
  for (auto &p : args)p->globally_register(m);
}
void tuple::global_unroll(std::ostream &os) {
  os << "push r12\n"
        "mov r12, rax\n";
  for (int i = 0; i < args.size(); ++i) {
    os << "mov rax, qword [r12" << (i ? std::string("+").append(std::to_string(i * 8)) : "") << "]\n";
    args.at(i)->global_unroll(os);
  }
  os << "pop r12\n";
}

void tuple::ir_locally_unroll(ir::scope &s, ir::lang::var block) {
  using namespace ir::lang;
  for (size_t i = 0; i < args.size(); ++i) {
    var content = s.declare_assign(block[i + 2]);
    args.at(i)->ir_locally_unroll(s, content);
  }
}

size_t tuple::locally_unroll(std::ostream &os, size_t stack_pos) {
  size_t rax_pos = stack_pos + stack_unrolling_dimension(); // The first free position in stack
  os << "mov qword [rsp-" << 8 * (rax_pos - stack_pos) << "], rax ; save rax\n";
  for (int i = 0; i < args.size(); ++i) {
    if (i)os << "mov rax, qword [rsp-" << 8 * (rax_pos - stack_pos) << "] ; restore rax\n";
    os << "mov rax, qword [rax" << (i ? std::string("+").append(std::to_string(i * 8)) : "") << "] "
       << "; tuple " << loc << " " << (i + 1) << " of " << args.size() << " \n";
    stack_pos = args.at(i)->locally_unroll(os, stack_pos);

  }
  return stack_pos;
}
size_t tuple::test_locally_unroll(std::ostream &os,
                                  size_t stack_pos,
                                  size_t caller_stack_pos,
                                  std::string_view on_fail) {

  size_t rax_pos = stack_pos + stack_unrolling_dimension(); // The first free position in stack
  os << "mov qword [rsp-" << 8 * (rax_pos - caller_stack_pos) << "], rax ; save rax\n";
  for (int i = 0; i < args.size(); ++i) {
    if (i)os << "mov rax, qword [rsp-" << 8 * (rax_pos - caller_stack_pos) << "] ; restore rax\n";
    os << "mov rax, qword [rax" << (i ? std::string("+").append(std::to_string(i * 8)) : "") << "] "
       << "; tuple " << (i + 1) << " of " << args.size() << " \n";
    stack_pos = args.at(i)->test_locally_unroll(os, stack_pos, caller_stack_pos, on_fail);

  }
  return stack_pos;
}
size_t tuple::unrolled_size() const {
  size_t s = 0;
  for (auto &m : args)s += m->unrolled_size();
  return s;
}
size_t tuple::stack_unrolling_dimension() const {
  size_t s = 0, d = 0;
  for (auto &m : args) {
    d = std::max(d, s + m->stack_unrolling_dimension());
    s += m->unrolled_size();
  }
  ++d;
  return d;
}
std::ostream &tuple::print(std::ostream &os) const {
  os << "(";
  bool comma = false;
  for (const auto &arg : args) {
    if (comma)os << ", ";
    comma = true;
    arg->print(os);
  }
  return os << ")";
}
ir::lang::var tuple::ir_test_unroll(ir::scope &main, ir::lang::var v) {
  using namespace ir::lang;

  scope *current = &main;
  for (size_t i = 0; i < args.size(); ++i) {
    var this_okay = args.at(i)->ir_test_unroll(*current, current->declare_assign(v[2 + i]).mark(trivial));
    if (i + 1 == args.size()) {
      current->ret = this_okay;
    } else {
      current->push_back(instruction::cmp_vars{.v1=this_okay, .v2=current->declare_constant(2), .op=instruction::cmp_vars::test});
      rhs_expr::branch b = std::make_unique<ternary>();
      b->cond = ternary::jz;
      b->jmp_branch.ret = b->jmp_branch.declare_constant(1); //false
      scope *next = &b->nojmp_branch;
      current->ret = current->declare_assign(std::move(b));
      current = next;
    }
  }
  return main.ret;
}

size_t literal::test_locally_unroll(std::ostream &os,
                                    size_t stack_pos,
                                    size_t caller_stack_pos, std::string_view on_fail) {
  os << "cmp rax, " << value->to_value() << " ; literal " << loc << "\n";
  os << "jne " << on_fail << " \n";
  return stack_pos;
}
ir::lang::var literal::ir_test_unroll(ir::scope &s, ir::lang::var v) {
  using namespace ir::lang;
  s.push_back(instruction::cmp_vars{.v1=v, .v2=s.declare_constant(value->to_value()), .op=instruction::cmp_vars::cmp});
  scope strue, sfalse;
  strue.ret = strue.declare_assign(3);
  sfalse.ret = sfalse.declare_assign(1);
  return s.declare_assign(std::make_unique<ternary>(ternary{.cond = ternary::jne, .nojmp_branch=std::move(strue), .jmp_branch=std::move(
      sfalse)}));
}

}
namespace literal {
uint64_t integer::to_value() const {
  return uint64_t((value << 1) | 1);
}
ir::lang::var integer::ir_compile(ir_sections_t s) const {
  return s.main.declare_constant(to_value());
}
uint64_t boolean::to_value() const { return value ? 3 : 1; }
ir::lang::var boolean::ir_compile(ir_sections_t s) const {
  return s.main.declare_constant(to_value());
}
ir::lang::var unit::ir_compile(ir_sections_t s) const {
  return s.main.declare_constant(to_value());
}
uint64_t string::to_value() const { THROW_UNIMPLEMENTED; }

ir::lang::var string::ir_compile(ir_sections_t s) const {
  assert(value.size() % 8 == 0);
  assert(value.size());
  //Let's make a static block
  static size_t id_factory = 0;
  size_t id = ++id_factory;
  std::string name("__string_literal_");
  name.append(std::to_string(id)).append("__");
  s.data << name << " dq 0, " << make_tag_size_d(Tag_String, value.size() / 8, 0) << "\n";
  s.data << " db ";
  bool is_string_open = false;
  bool comma = false;
  for (char c : value)
    if (chars::is_escaped_in_asm_string_literal(c)) {
      if (is_string_open)s.data << "\"";
      is_string_open = false;
      if (comma)s.data << ",";
      comma = true;
      s.data << int(c);
    } else {
      if (!is_string_open) {
        if (comma)s.data << ",";
        comma = true;
        s.data << "\"";
        is_string_open = true;
      }
      s.data << c;
    }
  if (is_string_open)s.data << "\"";
  s.data << "\n";
  return s.main.declare_global(name);
}
}

}
