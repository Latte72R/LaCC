# Latte’s C Compiler

LCC is a minimalist C compiler that implements only the core language features you need to get simple C programs running.

## Supported Features

### 1. Data Types

- **Primitive**: `int`, `char`, `void`  
- **Derived**: pointer types (`T*`), arrays (`T[]`)  
- **Composite**: structures (`struct`), enumerations (`enum`)  

### 2. Functions

- **Definition**: specify parameter and return types  
- **Declaration & Invocation**: define and call functions; use `return` to send back a value

### 3. Global & Local Variables

Both global and local (stack) variable declarations are supported.

### 4. Control Flow

- **Conditional Branching**  
  - `if (condition) { … }`  
  - `else { … }`  
- **Loops**  
  - `for (init; condition; step) { … }`  
  - `while (condition) { … }`  
- **Loop Control**  
  - `break` exits a loop  
  - `continue` skips to the next iteration

### 5. Operators

* **Arithmetic**: `+`, `-`, `*`, `/`, `%`
* **Relational**: `==`, `!=`, `<`, `<=`, `>`, `>=`
* **Logical**: `&&`, `||`, `!`
* **Bitwise**: `&`, `|`, `^`, `~`, `<<`, `>>`

## Unsupported Constructs

LCC does **not** support the following:

- Nested functions (functions defined within other functions)  
- `switch` statements and `case`/`default` labels  
- `goto` statement  
- `do-while` loops  
- Ternary conditional operator (`?:`)  
- `union` types  
- Extended primitive types: `unsigned`, `long`, `float`, `double`, etc.  
- Type qualifiers & storage-class specifiers: `const`, `volatile`, `static`, `register`, `auto`, etc.  
- No initializer lists for arrays or structs (e.g., `int arr[] = {2, 4, 6};` or `struct AB p = {.a = 1, .b = 2};`)  
- Inline assembly  
- Preprocessor directives: `#include`, `#define`, `#ifdef`, etc.  
- Variadic functions (functions with `...` parameters)  


## Limitations

### Single‐Unit Compilation
LCC only handles one .c file at a time — there's no support for separate compilation or linking multiple translation units.

### No Optimizations
There are no code-generation optimizations beyond what's needed to make it work.
