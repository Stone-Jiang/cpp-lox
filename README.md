**This is a C++ implementation of the bytecode version of Lox.**


## Key Features
1. Components are organized in classes with little abstraction cost, i.e. no `virtual` pointers, no dynamic dispatch, etc, so it's almost as fast as the original design.
2. The C++ version improved ownership problems and visibility issues.
3. Replaced macros for constants and helper functions with `constexpr` or inline functions, which is almost as efficient.
4. Instances of `VM`, `Compiler`, and `Scanner` are singleton but not global variables, which leaves future possibilities of extensions.
5. The original hash table design is replaced with `std::unordered_map<>` if `BETTER_HASH_TABLE` macro is off and `robinhood::flat_table` if `BETTER_HASH_TABLE` macro is on.
6. Extended repl and small commands: `-r` to run a file.


## Additional Features
1. `break` and `continue` keywords as separate loop control statements.
2. Improved adding native functions with minimal, non-invasive changes.
3. Added "'" to be part of a legal name.
4. Passed a reference of `VM` into native functions so that clear error messages can be reported.
5. Added native complex numbers as a part of number features, kept as an object instead of `Value`, keeping the same cache efficiency.
6. Added scanning, compiling and computing complex numbers. Real numbers are automatically upgraded when computing with complex numbers. 
7. Added numerous native mathematical functions with normal versions for real numbers, and apostrophe'd versions for complex numbers.
8. Added static methods in classes declared as `static method()`, which can be called by `instance.method()` and also `Class.method()`. 
9. Runtime patching methods is allowed (`Class.method = func`), the cost is runtime lookup of static methods.


## TODO List
1. Urgent: Memory on stack
2. Internal magic methods
3. More but safe syntactic sugars
   1. `+=`
   2. `++` as statement instead of expression
   3. lambdas/anon functions maybe with `lambda` or perhaps `\`?
4. A separate `int` type?
5. Native containers
   1. Native arrays and their functions
   2. Native aligned matrices and their math functions.
   3. Native map structure
6. Templated helper functions
7. Possible type/meta info like class name or typename at runtime.
8. Minimal reflections:
   1. `eval()`
   2. unsafe `exec()`
   3. unsafe getting and setting fields by strings
9.  Possible concurrency model?
10. Possible importing and helping commands? Maybe like `:import`
11. Maybe improvements on GC but I'm not sure.
12. Maybe support for unicode?
13. Better repl behavior: automatic printing & emitted semicolon
14. {}-style formatting strings and maybe %-style formatting strings
15. Opening and closing files
16. Flexible number of arguments & default value passing
17. 