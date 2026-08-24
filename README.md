**This is a C++ implementation of the bytecode version of Lox.**

Requires C++ 20 or higher.

## Key Features
1. Components are organized in classes with little abstraction cost, i.e. no `virtual` pointers, no dynamic dispatch, etc, so it's almost as fast as the original design.
2. The C++ version improved ownership models and visibility issues.
3. Replaced macros for constants and helper functions with `constexpr` or inline functions, which is almost as efficient.
4. Instances of `VM`, `Compiler`, and `Scanner` are not global variables, which leaves future possibilities of extensions.
5. The original hash table design is replaced with `std::unordered_map<>` if `BETTER_HASH_TABLE` macro is off and `robin_hood::unordered_flat_map` if `BETTER_HASH_TABLE` macro is on.
6. Uses `std::array<>` for fixed-length arrays and a custom wrapped-up `Vector<>` for dynamic array management, making deconstruction safe at the end.
7. Extended repl and small commands: `-r` to run a file.
8. Because I didn't use cmake or make, I used a python script to build files incrementally (`build.py`).


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

