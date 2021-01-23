#ifndef COMPILERS_BML_LIB_BUILDER_BUILD_H_
#define COMPILERS_BML_LIB_BUILDER_BUILD_H_

#include <parse/parse.h>

void resolve_global_free_vars(ast::free_vars_t &&fv, const ast::global_map &m);

void build_direct(std::string_view s, std::ostream &target);


void build_ir(std::string_view s, std::ostream &target, std::string_view filename = "source.ml");
/*
 IDEA for tests:
 1. let (a,b) = fun () -> 3 ;;  // Error: This expression should not be a function, the expected type is 'a * 'b
 2. let rec x = a () and y = 3 + 4 and z = Some x ;; // OK
 Int a let rec, every right hand side should be either free from siblings or 
 */

#endif //COMPILERS_BML_LIB_BUILDER_BUILD_H_
