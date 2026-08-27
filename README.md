**Craft is a C++20 implementation of the bytecode VM from [*Crafting Interpreters*](https://craftinginterpreters.com/), extended beyond the original C implementation of clox.**

The scanner, Pratt parser/compiler, bytecode VM, closures, classes, string interning, and mark-and-sweep garbage collector retain the overall clox architecture. The implementation uses C++ types and lifetime management while keeping the runtime based on compact tagged values and objects rather than a virtual C++ class hierarchy.

## Requirements and Building

- Probably a 64-bit machine (not tested on 32-bit machine)
- Python 3 (for playground and incremental build script)
- A C++ **20** C++ compiler available as `g++`, `clang++`, or `MSVC`.

The included incremental build script discovers sources under `src/`, tracks transitive local-header dependencies, and compiles independent files in parallel (not required, can be replaced with `make` or `cmake`). 

```sh
python build.py                 # optimized release build
python build.py --main          # optimized release build, but explicit
python build.py --debug         # debug build with tracing and GC diagnostics
python build.py --both          # build both profiles
python build.py -j 8            # use eight compilation workers
python build.py --clean         # remove generated objects and executables
```

The release executable is named `main` (`main.exe` on Windows), and the debug executable is named `debug` (`debug.exe` on Windows). Use `--cc` to select a specific compiler.

## Differences from the Original C Implementation

#### C++ runtime structure

- Core runtime concepts are represented by classes and strongly typed scoped enums instead of groups of C structs, globals, and loosely related helper functions.
- Fixed-capacity VM storage uses `std::array`. Growable bytecode, constant, line, and upvalue storage uses a project `Vector<T>` implementation with complete construction, destruction, copy, move, and iteration behavior.
- VM-owned containers report capacity changes to the runtime, allowing their backing storage to participate in garbage-collection allocation accounting.
- Constants and type mappings use `constexpr`, inline functions, concepts, and templates where the C version relies heavily on preprocessor macros.
- Runtime objects retain explicit `ObjType` tags. They do not use virtual methods or RTTI for dispatch, preserving the tagged-object model used by clox.
- Runtime diagnostics use `std::format` and C++ exceptions are reserved for internal failures such as invalid access, overflow, and allocation-related errors. Lox-level failures are represented separately as runtime values.
- `VM`, `Compiler`, and `Scanner` are not global variables or singletons, leaving space for extensions in the future.

#### Configurable value and table implementations

`src/switches.h` controls two important implementation choices:

- With `NAN_BOXING` enabled, `Value` uses a compact 64-bit NaN-boxed representation. Without it, the same public interface is backed by `std::variant<std::monostate, double, bool, Obj*>`.
- With `BETTER_HASH_TABLE` enabled, globals, fields, members, and interned strings use bundled robin-hood flat hash containers. Without it, they use `std::unordered_map` and `std::unordered_set`.
- String interning supports heterogeneous lookup by `std::string_view`, so a temporary heap string is not required merely to search the intern pool.

#### Native-function interface

Native functions are registered through compile-time `NativeDef` tables containing a name, arity, and function pointer. Each native receives the active `VM&`, which lets it allocate VM-owned objects and return detailed failures. `NativeResult` distinguishes success from failure and can carry an error kind, message, and optional Lox payload.

Adding a native therefore does not require modifying the VM's call dispatch: define the function and add one entry to a native-definition table.

#### Runtime and command-line behavior

- Supplying one path runs that source file directly.
- Starting the executable without arguments opens a persistent REPL.
- Inside the terminal REPL, `-r path/to/file.lox` runs a file using the current
  VM instance.
- File, compile, and runtime failures use distinct exit codes (74, 65, and 70 respectively).

## Language Extensions

#### Loop control

`break` and `continue` are supported in `while` and `for` loops, including nested loops. The compiler rejects either keyword outside a loop and prevents loop control from crossing a function boundary.

#### Complex numbers and mathematics

- Imaginary literals such as `4i` are scanned and compiled directly.
- Arithmetic promotes real numbers when either operand is complex.
- Complex values are heap objects containing `std::complex<double>` rather than another alternative in `Value`; this preserves the compact scalar value representation.
- Real math natives include `sqrt`, trigonometric and hyperbolic functions, logarithms, exponentiation, and inverse functions.
- Apostrophe-suffixed functions such as `sqrt'`, `sin'`, and `acos'` accept complex input. Apostrophes are valid identifier characters for this purpose.
- `real`, `imag`, `abs`, `arg`, `mag`, and `conj` expose common complex-number operations.
- Native math functions validate types, domains, ranges, and finite results instead of silently passing invalid results through the VM.
- The numeric system directly uses C++ library functions with the same behavior, meaning that mathematically true expressions like `sin(1)/cos(1)==tan(1)` might evaluate to false. The only small change is that a value near zero (by 1e-11) is approximated to zero.

#### Error values and recovery

Native functions and user defined functions can produce typed error values. This error-handling system is inspired by Rust/Haskell, in which when an error happens, an `ObjError` object is returned. An error is automatically unpacked if no error happens. If an error is unhandled and used directly (except for printing, reading meta info like `typeof()`, or functions specifically for handling functions), the VM screams and aborts the program.

The infix `else` expression (sugar) keeps a successful value and evaluates its right-hand fallback only when the left side is an error. Using an unhandled error in an incompatible operation produces a runtime diagnostic that retains the original error category and message.

```
var a = sqrt(4);
print a; // 2

var a = sqrt(-4);
print a; // <err DOMAIN: sqrt() is undefined for negative real numbers.>

var a =sqrt(-4) else 0;
print a; // 0 (fallback)

var a = sqrt(-4)+1;
// [runtime error] Can't use an error value in addition. Original error [DOMAIN]: sqrt() is undefined for negative real numbers.
```

Users can return a custom error, where the supported categories are `DOMAIN`, `RANGE`, `TYPE`, `INDEX`, `IO`, `VALUE`, `NAME`, and `USER`; an error may also carry a payload representing the actual value.

```
fun requireNonNegative(value) {
  if (value < 0) 
    fail DOMAIN, "Expected a non-negative number.", value;
  return value;
}

print requireNonNegative(-1) else 0;
```

#### Static methods and mutable class members

Methods declared with `static` can be called through a class or an object:

```
class Counter {
  init(value) {
    this.value = value;
  }

  static create() {
    return Counter(0);
  }
}
```

Static methods participate in inherited member lookup. Instance methods remain bound to a receiver, and the compiler rejects `this` or `super` in static context. Class members can be replaced at runtime, for example `Counter.create = anotherFunction`.

#### Runtime type and meta information

The `typeof(value)` native reports primitive and callable categories, classes, errors, and the concrete class name of an instance.

#### Arrays

Arrays are dynamically sized, mutable objects whose elements remain dynamically typed. Whitespace is insignificant in literals, and a trailing comma is allowed.

```
var empty = [];
var values = [1, "two", true, nil];
var trailing = [1, 2, 3,];
var nested = [[1, 2], [3, 4], []];
```

Array variables have reference semantics. Assignment aliases the same array rather than copying its elements:

```
var a = [1, 2, 3];
var b = a;

b[0] = 10;
print a; // [10, 2, 3]
```

Indexes must be finite integral numbers. Integral floating-point values such as `1.0` are accepted, and negative indexes count backward from the end. Invalid or out-of-range indexes produce an `INDEX` error value.

```
var a = [10, 20, 30];

print a[1.0]; // 20
print a[-1];  // 30
print a[-3];  // 10

a[-1] = 99;
print a; // [10, 20, 99]

print a[10] else "missing"; // missing
```

The read-only `len` attribute reports the current number of elements. Capacity and other storage details remain internal to the VM.

```
var a = [1, 2];
print a.len; // 2
```

Arrays currently provide these mutating native methods:

- `push(value)` appends a value and returns `nil`.
- `pop()` removes and returns the final element; popping an empty array produces an `INDEX` error.
- `insert(index, value)` inserts before the selected position and returns `nil`. Position `len` appends, and negative positions count backward from the end.
- `clear()` removes every element and returns `nil`.

```
var a = [1, 3];

a.insert(1, 2);
a.push(4);
print a;       // [1, 2, 3, 4]
print a.pop(); // 4

a.clear();
print a.len;   // 0
```

`copy()` explicitly creates a new outer array. The copy is shallow: nested arrays and other object elements remain shared references.

```
var inner = [1];
var a = [inner, 2];
var b = a.copy();

b[1] = 3;
print a; // [[1], 2]
print b; // [[1], 3]

b[0].push(4);
print a; // [[1, 4], 2]
```

Pushing an array directly into itself is rejected with a `VALUE` error, including through an alias. Use `copy()` when a snapshot of the current outer array is intended:

```
var a = [1, 2, 3];
var alias = a;

print a.push(a) else "self-reference rejected";
print a.push(alias) else "alias rejected";

a.push(a.copy());
print a; // [1, 2, 3, [1, 2, 3]]
```

Arrays participate in garbage-collector tracing: objects stored only inside a reachable array remain alive. Ordinary assignment, argument passing, and return values continue to share the same array object unless `copy()` is called explicitly.

## Playground

The local web playground provides a source editor, program output, and a persistent REPL. It uses the existing Craft executable, so build the project before starting the playground.

From the project root, run:

```sh
python playground/server.py
```

Then open <http://127.0.0.1:8765> in a browser.

#### Running a source file

1. Select **Open file** in the toolbar above the left pane.
2. Choose a `.lox` or plain-text source file from your computer.
3. Review or edit the uploaded code in the left pane.
4. Select **Run**, or press <kbd>Ctrl</kbd>+<kbd>Enter</kbd> (<kbd>Cmd</kbd>+<kbd>Enter</kbd> on macOS).
5. Read the program output and exit status in the right pane.

#### Using the REPL

1. Select the **REPL** tab in the right pane.
2. Enter a Craft expression or statement and select **Send**.
3. Continue entering commands in the same persistent session.
4. Select **Reset** to discard the current REPL state and start a fresh session.

#### Server options

By default, the server uses `main.exe`, binds to `127.0.0.1`, and listens on port `8765`. A different executable or port can be selected when starting it:

```sh
python playground/server.py --exe debug.exe --port 9000
```

Use `python playground/server.py --help` to see all available server options.
