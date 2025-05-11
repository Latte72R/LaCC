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

### 3. Control Flow

- **Conditional Branching**  
  - `if (condition) { … }`  
  - `else { … }`  
- **Loops**  
  - `for (init; condition; update) { … }`  
  - `while (condition) { … }`  
- **Loop Control**  
  - `break` exits a loop  
  - `continue` skips to the next iteration  

---

## Unsupported Constructs

LCC does **not** support the following:

- Nested functions (functions defined within other functions)  
- `switch` statements and `case`/`default` labels  
- `do-while` loops  
- Bitwise operators: `&`, `|`, `^`, `~`, `<<`, `>>`  
- Ternary conditional operator (`?:`)  
- `union` types  
- Extended primitive types: `unsigned`, `long`, `float`, `double`, etc.  
- Type qualifiers & storage-class specifiers: `const`, `volatile`, `static`, `register`, `auto`, etc.  
- Inline assembly  
- Preprocessor directives: `#include`, `#define`, `#ifdef`, etc.  
- Direct struct initialization syntax (e.g., `struct point p = {1, 2};`)  
- Variadic functions (functions with `...` parameters)  
