/*

 Let's discuss some dynamics here a little bit.

  1) type definition is completely code-agnostic. let's sort that out first.

  2) In any point in code, we can make lookups. This just boil down to
    - look up an identifier (possibly specialized with module system, but that'll come in later)
    - look up a constructor (from type system)
    Identifiers are introduced by matchers, Constructors by type system.
    For types we certainly want to have an instance-representation in C++ (i.e. for each primitive or declared type, a C++ instance describing that) and
    we also want a way to look up constructors. (The latter is easy, as we go with a linear structure, hence map : std::string_view -> [type or constructor ? still have to decide]
    For identifier, as they are introduced by universal, we might go with a tree map (even though we just use an inner line hence some good trick could be done)
    For each universal, we want to know where that is going to stored. That is going to be either at an absolute (virtual) address for global stuff,
    and a stack-relative address for scope-relative stuff (function parameters, closure parameters, locally introduced values)

    Things can get messy with closures. e.g.

    let f a b c =
      let g x y =
        let b = x + y in
        a + b + x + y
      in  // g has starting stack a | x y | (b not closured)
      g (g a b) (g b c)


    let f a b c =
      let g x y =
        let h x y =
          c + x + y
        in
        let b = x + y in
        h (a + b + x + y)
      in  // g has starting stack a c | x y | (b not closured)
      ...

      for every expression in the form let f [some_args] = e in ... where a name x is defined and x is mentioned below (even in sub-methods) then x is part of the closure of f.
      Algorithm for determining closures set:
      Step 1. (Top-down) Take care of the globals i.e. eliminate them
      Step 2. (Bottom-up) Keep the set of free variables going up. When something is mentioned, remove that. When traversing a function definition, add those variables to the closure.

      We can better refactor this in more indirections (no one likes strings)
      First, top-down assign ids to every name. At this point we also know enough to determine who is global and who is not.
      Secondly, bottom-up to find whether some name is captured or not. The tricky things: the memory location of something is capture-dependent, its type is not.
      This makes easy to conclude:
        within introduction/matcher store type and original location;
        within usages store link to introduction and [`Direct | `Closured of single_function_definition]
        within function definition store a map from closures to their closured_location
      store the type within the introduction/matcher; on each usage store information on how to get

      type location = | Absolute of address | Stack_relative of int

      type type = | Primitive of primitive_type |








 */
