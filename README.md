**This is a C++20 implementation of the bytecode VM of (a superset of) Lox from [*Crafting Interpreters*](https://craftinginterpreters.com/), extended beyond the original C implementation of clox.**

The scanner, Pratt parser/compiler, bytecode VM, closures, classes, string interning, and mark-and-sweep garbage collector retain the overall clox architecture. 

The implementation uses C++ types and lifetime management while keeping the runtime based on compact tagged values and objects rather than a virtual C++ class hierarchy.

## Requirements and Building

[!IMPORTANT]
- A C++ **20** compiler available as `g++`, `clang++`, or `MSVC`.
- Probably a 64-bit machine (not tested on 32-bit machines)
- Python 3 (for custom build & playground.) **Not required for if you don't need these two features.**

The included incremental build script discovers sources under `src/`, tracks transitive local-header dependencies, and compiles independent files in parallel (not required, can be replaced with `make` or `cmake`). 

```sh
python build.py                 # optimized release build
python build.py --main          # optimized release build, but explicit
python build.py --debug         # debug build with tracing and GC diagnostics
python build.py --both          # build both profiles
python build.py -j 8            # use eight compilation workers
python build.py --clean         # remove generated objects and executables
```

The release executable is named `main`, and the debug executable is named `debug`. Use `--cc` to select a specific compiler.

## Differences from the Original C Implementation

### C++ runtime structure

- Core runtime concepts are represented by C++ classes, strongly typed enums, and templated functions instead of groups of C structs, enums, and loosely related helper functions.
- Runtime objects retain explicit `ObjType` tags. They do not use virtual methods or RTTI for dispatch, preserving the tagged-object model used by clox.
- Constants and type mappings use `constexpr`, inline functions, concepts, and templates where the C version relies heavily on preprocessor macros. Under optimization, these C++ expressions are expanded at compile-time, causing no runtime costs.
- Replaced some pointer passing with references, giving more accurate semantics and clearer ownership models.
- Fixed-capacity VM storage uses `std::array`. Growable bytecode, constant, line, and upvalue storage uses a project `Vector<T>` implementation with complete construction, destruction, copy, move, and iteration behavior.
- VM-owned containers report capacity changes to the runtime, allowing their backing storage to participate in garbage-collection allocation accounting.
- Runtime diagnostics use `std::format` and C++ exceptions are reserved for internal failures such as invalid access, overflow, and allocation-related errors. Lox-level failures are represented separately as runtime values.
- `VM`, `Compiler`, and `Scanner` are not global variables or singletons, leaving space for extensions in the future.
- Replaced messages and string handling with `std::string` and `std::string_view`, which provides simpler and more readable memory management with low cost.
- Added a `to_string` layer to printing in Lox, which can be more easily extended, pipelined, and integrated with other components.
- Added colors in debug mode. In debug mode, **please use `-q` to quit!** Using `ctrl c` or other OS level exit might make your terminal cyan, though not harmful.

### Configurable value and table implementations

`src/switches.h` controls two important implementation choices:

- With `NAN_BOXING` enabled, `Value` uses a compact 64-bit NaN-boxed representation. Without it, the same public interface is backed by `std::variant<std::monostate, double, bool, Obj*>`.
- With `BETTER_HASH_TABLE` enabled, globals, fields, members, and interned strings use bundled [robin-hood flat hash containers](https://github.com/martinus/robin-hood-hashing). Without it, they use `std::unordered_map` and `std::unordered_set`.
- String interning supports heterogeneous lookup by `std::string_view`, so a temporary heap string is not required merely to search the intern pool.

### Native-function interface

Native functions are registered through compile-time `NativeDef` tables containing a name, arity, and function pointer. Each native receives the active `VM&`, which lets it allocate VM-owned objects and return detailed failures. `NativeResult` distinguishes success from failure and can carry an error kind, message, and optional Lox payload.

Adding a native therefore does not require modifying the VM's call dispatch: define the function and add one entry to a native-definition table.

### Runtime and command-line behavior

- Supplying one path runs that source file directly.
- Starting the executable without arguments opens a persistent REPL.
- Inside the terminal REPL, `-r path/to/file.lox` runs a file using the current VM instance.
- Inside the terminal REPL, enter `-q` to quit.

## Language Extensions

### Loop control

`break` and `continue` are supported in `while` and `for` loops, including nested loops. The compiler rejects either keyword outside a loop and prevents loop control from crossing a function boundary.

Collection iteration uses a separate `range` statement. Parentheses are required, and the collection expression is evaluated once:

```lox
range (value : array) { print value; }
range (index, value : array) { print index; print value; }

range (key : map) { print key; }
range (key, value : map) { print key; print value; }
```

An array supplies its values to the one-variable form and zero-based index/value pairs to the two-variable form. A map supplies keys or key/value pairs. Map traversal order is unspecified and does not preserve insertion order.

Iteration takes a shallow snapshot before entering the loop. Adding or removing collection entries during the body therefore does not change which entries the current loop visits, although referenced mutable values remain shared. The iterator variables are scoped to the loop, and `break` and `continue` work as they do in other loops. Supplying anything other than an array or map is a runtime error.

### Complex numbers and mathematics

- Imaginary literals such as `4i` are scanned and compiled directly.
- Arithmetic promotes real numbers when either operand is complex.
- Complex values are heap objects containing `std::complex<double>` rather than another alternative in `Value`; this preserves the compact scalar value representation.
- Real math natives include `sqrt`, trigonometric and hyperbolic functions, logarithms, exponentiation, and inverse functions.
- Apostrophe-suffixed functions such as `sqrt'`, `sin'`, and `acos'` accept complex input. Apostrophes are valid identifier characters for this purpose.
- `real`, `imag`, `abs`, `arg`, `mag`, and `conj` expose common complex-number operations.
- Native math functions validate types, domains, ranges, and finite results instead of silently passing invalid results through the VM.
- The numeric system directly uses C++ operations and library functions, meaning that there are floating-point inaccuracies and mathematically true expressions like `sin(1)/cos(1)==tan(1)` might evaluate to false. The only small change is that a value near zero (by 1e-11) is approximated to zero. If you want to compare whether two numbers are equal under a margin of error, use `~=`.

### Error values and recovery

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

### Static methods and mutable class members

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

### Native functions

- `typeof(value)` native reports primitive and callable categories, classes, errors, and the concrete class name of an instance.
- `system(string)` native runs a system command through C++ `system()`. **UNSAFE!! MIGHT CAUSE SERIOUS CRASHES! DO NOT PASS IN UNVERIFIED COMMANDS.**
- `str(value)` converts everything into a string as how they would be printed.

### Strings

Strings are immutable and interned, same as the original design. Native string transformations build independent temporary storage and return an immutable string result; they never modify the receiver. Interning may reuse an existing object when the resulting text already exists, which is not observable through string value semantics.

The read-only `len` attribute reports the number of stored bytes. The initial native methods are:

- `upper()` and `lower()` return ASCII/C-locale case transformations.
- `trim()` returns a string without leading or trailing C-locale whitespace.
- `reverse()` returns a string with its stored bytes reversed.
- `substr()` takes two *indices* and returns the substring at `[lo, hi)`.
- `find()` finds the first occurrence of substring from the left; `rfind()` from the right.
- `startswith()` and `endswith()` returns whether the string starts/ends with the given substring.

```
var original = "  Ab C  ";

print original.upper();   //   AB C
print original.trim();    // Ab C
print original.reverse(); //   C bA
print original;           //   Ab C
```

New native string implementations belong in `src/lib/strings.cpp`. `makeStringResult()` centralizes conversion to an interned Lox string and translates allocation failures into `CRITICAL` native errors.

### Arrays

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

Arrays provide these native methods:

| Interface | Return value | Mutates receiver? | Behavior |
|---|---|---:|---|
| `a.push(value)` | `nil` | Yes | Appends `value` |
| `a.pop()` | Removed final element | Yes | Produces an `INDEX` error when empty |
| `a.insert(index, value)` | `nil` | Yes | Inserts before `index`; position `len` appends, and negative positions count backward from the end |
| `a.clear()` | `nil` | Yes | Removes every element |
| `a.copy()` | New shallow array | No | Copies the outer storage; nested objects remain shared |
| `a.deep_copy()` | New recursively copied array | No | Recursively copies nested arrays and maps while preserving aliases and cycles in the copied graph |
| `a.reverse()` | `nil` | Yes | Reverses the elements in place |
| `a.concat(other)` | `nil` | Yes | Appends the elements of `other` |
| `a.erase(index)` | `nil` | Yes | Removes the element at an integral index |
| `a.remove(value)` | Boolean | When found | Removes the first equal value and reports whether one was found |
| `a.front()` | First element | No | Produces an `INDEX` error when empty |
| `a.back()` | Last element | No | Produces an `INDEX` error when empty |
| `a.count(value)` | Number of equal elements | No | Counts elements equal to `value` |
| `a.find(value)` | First matching index, or `-1` | No | Searches from the beginning |
| `a.slice(lo, hi)` | New shallow array | No | Copies the half-open range `[lo, hi)`; `hi` may equal `len` |
| `a.join(separator)` | New string | No | Converts elements to strings and joins them with `separator` |
| `a.sort()` | `nil` | Yes | Sorts in place using the runtime's ordinary value ordering |
| `a == other` | Boolean | No | Returns whether `a` and `other` *are the same* arrays; 
| `a ~= other` | Boolean | No | Returns whether `a` and `other` element-wise equal arrays $O(n)$; 

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

Higher-order array operations are implemented in `src/lib/arrays.lox`, which is loaded once before a file is executed or a REPL session starts. The functions remain callable globally, and selected functions are also registered as array extension methods. `map` returns a new array, while `foreach` replaces the elements of the original array:

```
var values = [1, 2, 3];

print map(values, \x => x * 2);    // [2, 4, 6]
print values.map(\x => x + 1);     // [2, 3, 4]
values.foreach(\x => x * 10);
print values;                      // [10, 20, 30]
```

The script library currently provides:

| Preferred method | Global form | Return value | Mutates receiver? | Behavior |
|---|---|---|---:|---|
| `array.map(fn)` | `map(array, fn)` | New array | No | Applies `fn(element)` to every element |
| `array.foreach(fn)` | `foreach(array, fn)` | Original array | Yes | Replaces every element with `fn(element)` |
| `array.filter(predicate)` | `filter(array, predicate)` | New array | No | Keeps elements accepted by `predicate` |
| `array.fill(value)` | `fill(array, value)` | Original array | Yes | Replaces every element with `value` |
| `array.foldl(initial, fn)` | `foldl(array, initial, fn)` | Accumulated value | No | Folds from left to right |
| `array.foldr(initial, fn)` | `foldr(array, initial, fn)` | Accumulated value | No | Folds from right to left |
| `array.any(predicate)` | `any(array, predicate)` | Boolean | No | Tests elements with short-circuiting until one matches |
| `array.all(predicate)` | `all(array, predicate)` | Boolean | No | Tests elements with short-circuiting until one fails |
| `array.find'(predicate)` | `find'(array, predicate)` | First matching index, or `nil` | No | Finds the first element accepted by `predicate` |
| `array.count'(predicate)` | `count'(array, predicate)` | Number of matching elements | No | Counts elements accepted by `predicate` |
| `array.sort'(comparator)` | `sort'(array, comparator)` | Original array | Yes | Sorts in place using a two-argument comparator |
| `array.intersperse(value)` | `intersperse(array, value)` | New array | No | Places `value` between adjacent elements |
| — | `zip(fn, array1, array2)` | New array | No | Combines corresponding elements, stopping at the shorter input; global-only |

The read-only `arity` attribute reports a function's fixed parameter count. The standard-library methods use it to validate callbacks even when an array is empty.

### Lambdas / Anonymous functions

A lambda is defined in Haskell-fashion as `var a = \x => x + 1;`. They follow the same rules as functions in terms of being first-class, capable of assignment, capturing closures etc., but they can be anonymous. They can also be used on-spot.

These are correct ways of defining and using lambdas:

```
// Zero parameters.
var one = \() => 1;
print one(); // expect: 1
print one; // expect: <fn (lambda)>

// Bare and parenthesized single parameters.
print (\x => x + 1)(2);   // expect: 3
print (\(x) => x + 1)(2); // expect: 3

// Multiple parameters.
print (\(x, y) => x + y)(2, 3); // expect: 5

// Capture.
fun makeAdder(n) {
  return \x => x + n;
}
print makeAdder(5)(10); // expect: 15

// Nested capture.
var make = \x => \y => x + y;
print make(2)(3); // expect: 5

// Mutation of captured state.
fun counter() {
  var n = 0;
  return \() => n = n + 1;
}

var next = counter();
print next(); // expect: 1
print next(); // expect: 2

// First-class storage.
var functions = [\x => x + 1];
print functions[0](9); // expect: 10
```

### Maps

`Map()` constructs an empty, mutable hash map. Map variables have reference semantics: assignment aliases the same map, while `copy()` and `deep_copy()` create new map objects. Capacity, load factor, buckets, and the selected C++ hash-table implementation remain internal.

```
var scores = Map();
scores["Ada"] = 10;
scores["Grace"] = 20;

var alias = scores;
alias["Ada"] = 15;
print scores["Ada"]; // 15
print scores.len;    // 2
```

Index assignment inserts a missing key or overwrites an existing key. The assignment expression evaluates to the assigned value. Index lookup returns the stored value, or harmless `nil` when the key is absent:

```
var m = Map();

print m["missing"]; // nil; no insertion
print m.len;        // 0

print m["x"] = 1; // 1; inserts "x"
print m["x"] = 2; // 2; overwrites "x"
print m.len;       // 1
```

`len` is a read-only numeric property. The native interfaces have the following return and update behavior:

| Interface | Return value | Mutates receiver? | Insertion or overwrite behavior |
|---|---|---:|---|
| `m[key]` | Stored value, or `nil` | No | Never inserts |
| `m[key] = value` | `value` | Yes | Inserts a missing key; overwrites an existing key |
| `m.has_key(key)` | Boolean | No | Never inserts |
| `m.get_or(key, fallback)` | Stored value, or `fallback` | No | Never inserts |
| `m.get_or_set(key, fallback)` | Stored value, or `fallback` | Only when missing | Inserts `fallback` only when the key is absent; never overwrites |
| `m.remove(key)` | Removed value, or `nil` | Only when found | Never inserts |
| `m.remove_or(key, fallback)` | Removed value, or `fallback` | Only when found | Never inserts |
| `m.clear()` | `nil` | Yes | Removes every entry |
| `m.keys()` | New array of keys | No | Never inserts |
| `m.values()` | New array of values | No | Never inserts |
| `m.items()` | New array containing `[key, value]` arrays | No | Never inserts |
| `m.copy()` | New shallow map | No | Copies all entries into independent outer storage |
| `m.deep_copy()` | New recursively copied map | No | Recursively copies array/map keys and values |
| `m.update(other)` | `nil` | Yes | Inserts missing keys and overwrites collisions with values from `other` |
| `m.merge(other)` | New shallow map | No | Starts with the receiver, then lets `other` overwrite collisions |
| `m.invert()` | New map | No | Uses values as keys; duplicate values overwrite earlier entries |

Hash-map traversal order is unspecified. Consequently, the order returned by `keys()`, `values()`, and `items()` is unspecified, and when `invert()` encounters duplicate values, which original key survives is also unspecified.

```
var defaults = Map();
defaults["color"] = "blue";

print defaults.get_or("size", 10);      // 10; unchanged
print defaults.get_or_set("size", 10); // 10; inserts
print defaults.get_or_set("size", 20); // 10; does not overwrite
print defaults.remove_or("size", -1);  // 10; removes
print defaults.remove_or("size", -1);  // -1
```

`copy()` is shallow: nested objects remain shared. `deep_copy()` recursively duplicates arrays and maps, including map keys, while preserving repeated references and cycles inside the new object graph. Strings, functions, files, instances, and other non-collection objects remain shared.

```
var inner = [1];
var source = Map();
source["items"] = inner;

var shallow = source.copy();
var deep = source.deep_copy();
inner.push(2);

print shallow["items"]; // [1, 2]
print deep["items"];    // [1]
```

#### Hash-key responsibility

[!WARNING]
Release builds provide a hash for every runtime value and do not reject object keys. Using mutable or otherwise unstable objects as keys is at the user's risk: changing state that participates in equality or hashing can make an entry surprising or unreachable. NaN is similarly unsafe because it is not equal to itself. Builds with `DEBUG_VALUE_TABLE` diagnose keys that are not considered stable.

`invert()` promotes every value to a key, so inversion carries the same responsibility. Inverting a map whose values are mutable, unstable, NaN, or duplicated can reject the operation in a debug build or produce overwrite/lookup behavior that depends on those values in a release build.

Maps strongly retain both keys and values for garbage collection. Map equality uses identity: two separately constructed maps are not equal merely because they contain equal entries.

#### Planned higher-order map operations

Higher-order map functions are not implemented yet. Their intended purposes and names are reserved clearly enough to guide the future API. Method form will be recommended; optional global forms will use the `map_` prefix to avoid collisions with array functions and `Map()`:

| Preferred method | Optional global form | Intended purpose |
|---|---|---|
| `m.foreach(fn)` | `map_foreach(m, fn)` | Visit each `(key, value)` pair for side effects |
| `m.filter(fn)` | `map_filter(m, fn)` | Return a new map containing pairs accepted by the predicate |
| `m.transform_values(fn)` | `map_transform_values(m, fn)` | Return a new map with transformed values and unchanged keys |
| `m.transform_keys(fn)` | `map_transform_keys(m, fn)` | Return a new map with transformed keys; collisions overwrite according to traversal order |
| `m.fold(initial, fn)` | `map_fold(m, initial, fn)` | Reduce all pairs to one accumulated value |
| `m.any(fn)` | `map_any(m, fn)` | Test whether any pair satisfies a predicate |
| `m.all(fn)` | `map_all(m, fn)` | Test whether every pair satisfies a predicate |

These names currently describe planned behavior only; calling them is not supported yet.

### Variadic functions

A final named parameter followed by `...` collects all remaining arguments into a fresh array. The rest array is created for every call, including calls with no extra arguments:

```
fun collect(first, rest...) {
  print first;
  return rest;
}

print collect(1);                // []
print collect(1, 2, "three");   // [2, three]

var tail = \(first, rest...) => rest;
print tail(10, 20, 30);          // [20, 30]
```

The parameter grammar is:

```
parameter-list -> IDENTIFIER ("," IDENTIFIER)* ["..."]
```

In practice, `...` must immediately follow the final parameter name. A rest parameter may be the only parameter, but it cannot be followed by another parameter:

```
fun collectAll(values...) {}       // valid
fun collect(first, rest...) {}     // valid
fun invalid(first..., later) {}    // compile error
```

The function's fixed arity excludes its rest parameter. Calling a variadic function requires at least that many fixed arguments; surplus arguments are packed into the rest array. The array is mutable, traces its elements for garbage collection, and can safely be captured by a closure.

Ellipsis is currently declaration syntax only. Calls do not yet support array spreading, so `fn(values...)` is not valid call syntax.

These are rejected by the compiler.

```
var bad = \ => 1;
var bad = \(a, a) => a;
var bad = \(a b) => a + b;
var bad = \(a,) => a;
var bad = \x, y => x + y;
var bad = \x x + 1;
```

### File I/O

`open(path, mode)` returns a garbage-collected file object backed by the native `File` wrapper. The mode is optional and defaults to `"r"`. Relative paths are resolved from the process's current working directory, so the examples below should be run from the project root. File objects expose stream operations only; filesystem controls such as deleting, renaming, or listing paths are not part of this API.

Supported modes use the usual `r`, `w`, and `a` forms. Adding `+` enables both reading and writing, and adding `b` selects binary mode. Opening with `w` truncates an existing file, while `a` writes at its end.

You can see more in examples/file_io.lox, which reads in the local readme.md file.

```
var file = open("README.md"); // equivalent to open("README.md", "r")

print file;          // <file README.md (r, o)>
print file.name;     // README.md
print file.mode;     // r
print file.readable; // true
print file.writable; // false
print file.closed;   // false
print file.size;     // size in bytes
print file.read(80); // read at most 80 bytes
print file.tell();   // 80
file.seek(0);        // return to the beginning
print file.readline();
file.close();
print file.closed;   // true
```

Writing uses the same object and mutates the underlying stream:

```
var output = open("notes.txt", "w");
print output.write("hello"); // 5
output.flush();
output.close();
var input = open("notes.txt");
print input.read(); // hello
input.close();
```

File attributes are read-only:
- `name` is the path supplied to `open()` and `mode` is the parsed mode string.
- `closed`, `readable`, `writable`, and `seekable` report the current stream capabilities.
- `eof` reports whether the stream has encountered end-of-file.
- `size` reports the file's size in bytes.

Methods are:
- `read()` reads the remainder; `read(n)` reads at most `n` bytes.
- `readline()` returns the next line or `nil` at end of file.
- `readlines()` returns the remaining lines as an array of strings.
- `write(string)` writes and returns the byte count.
- `writelines(array)` writes an array whose elements must all be strings.
- `seek(offset)` and `seek(offset, whence)` reposition the stream; `whence` is `0`, `1`, or `2` for start, current position, or end.
- `tell()` returns the current byte position.
- `flush()` and `close()` perform the corresponding stream operation and return `nil`.

`readline()` preserves a trailing newline when the source line has one. `readlines()` follows the same rule for each returned element. Calls made after `close()`, reads from a write-only stream, writes to a read-only stream, and failures to open a path produce `IO` error values. Invalid arguments become `TYPE`, `VALUE`, or `RANGE` errors.

The binding validates Lox values and delegates I/O and stream-state handling to the corresponding native `File` object. Assignment aliases the same file object; it does not duplicate an operating-system stream. A reachable file object keeps its stream alive, while an unreachable one is closed by RAII during garbage collection. Explicit `close()` remains recommended because garbage collection timing is intentionally unspecified.

### Script extension methods

Script functions can be registered as extension methods at top level. The registered implementation receives the method receiver as its first argument:

```
fun twice(fn, value) {
  return fn(fn(value));
}

extend "function" "twice" twice;
print (\x => x + 1).twice(40); // 42
```

Array, string, and function extension namespaces are available. Existing native methods cannot be replaced.

The grammar is:

```
extend-statement -> "extend" STRING STRING expression ";"
```

The first string must currently be `"array"`, `"string"`, or `"function"`; the second string is the method name, and the final expression must evaluate to a script closure. Registration captures that closure in a garbage-collector root. The closure must declare at least one fixed parameter for the receiver. Duplicate registrations and attempts to replace native methods are runtime errors.

Method dispatch checks native methods first and script extensions second. A direct call rearranges the receiver into the first ordinary argument:

```
value.method(a, b)
// equivalent to the registered implementation(value, a, b)
```

Extension sugar currently applies to direct calls. Merely reading an extension, such as `var method = value.method;`, does not create a bound function.

## Playground

The local web playground provides a source editor, program output, and a persistent REPL. It uses the existing Craft executable, so build the project before starting the playground.

From the project root, run:

```sh
python playground/server.py
```

Then open <http://127.0.0.1:8765> in a browser.

### Running a source file

1. Select **Open file** in the toolbar above the left pane.
2. Choose a `.lox` or plain-text source file from your computer.
3. Review or edit the uploaded code in the left pane.
4. Select **Run**, or press <kbd>Ctrl</kbd>+<kbd>Enter</kbd> (<kbd>Cmd</kbd>+<kbd>Enter</kbd> on macOS).
5. Read the program output and exit status in the right pane.

### Using the REPL

1. Select the **REPL** tab in the right pane.
2. Enter a Craft expression or statement and select **Send**.
3. Continue entering commands in the same persistent session.
4. Select **Reset** to discard the current REPL state and start a fresh session.

### Server options

By default, the server uses `main.exe`, binds to `127.0.0.1`, and listens on port `8765`. A different executable or port can be selected when starting it:

```sh
python playground/server.py --exe debug.exe --port 9000
```

Use `python playground/server.py --help` to see all available server options.
