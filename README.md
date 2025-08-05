English | [日本語](./README-ja.md)

![](./LaCC.png)

LaCC is a minimalist C compiler that implements only the core language features you need to get simple C programs running.

[![DeepWiki](https://img.shields.io/badge/DeepWiki-Latte72R%2FLaCC-blue.svg?logo=data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACwAAAAyCAYAAAAnWDnqAAAAAXNSR0IArs4c6QAAA05JREFUaEPtmUtyEzEQhtWTQyQLHNak2AB7ZnyXZMEjXMGeK/AIi+QuHrMnbChYY7MIh8g01fJoopFb0uhhEqqcbWTp06/uv1saEDv4O3n3dV60RfP947Mm9/SQc0ICFQgzfc4CYZoTPAswgSJCCUJUnAAoRHOAUOcATwbmVLWdGoH//PB8mnKqScAhsD0kYP3j/Yt5LPQe2KvcXmGvRHcDnpxfL2zOYJ1mFwrryWTz0advv1Ut4CJgf5uhDuDj5eUcAUoahrdY/56ebRWeraTjMt/00Sh3UDtjgHtQNHwcRGOC98BJEAEymycmYcWwOprTgcB6VZ5JK5TAJ+fXGLBm3FDAmn6oPPjR4rKCAoJCal2eAiQp2x0vxTPB3ALO2CRkwmDy5WohzBDwSEFKRwPbknEggCPB/imwrycgxX2NzoMCHhPkDwqYMr9tRcP5qNrMZHkVnOjRMWwLCcr8ohBVb1OMjxLwGCvjTikrsBOiA6fNyCrm8V1rP93iVPpwaE+gO0SsWmPiXB+jikdf6SizrT5qKasx5j8ABbHpFTx+vFXp9EnYQmLx02h1QTTrl6eDqxLnGjporxl3NL3agEvXdT0WmEost648sQOYAeJS9Q7bfUVoMGnjo4AZdUMQku50McDcMWcBPvr0SzbTAFDfvJqwLzgxwATnCgnp4wDl6Aa+Ax283gghmj+vj7feE2KBBRMW3FzOpLOADl0Isb5587h/U4gGvkt5v60Z1VLG8BhYjbzRwyQZemwAd6cCR5/XFWLYZRIMpX39AR0tjaGGiGzLVyhse5C9RKC6ai42ppWPKiBagOvaYk8lO7DajerabOZP46Lby5wKjw1HCRx7p9sVMOWGzb/vA1hwiWc6jm3MvQDTogQkiqIhJV0nBQBTU+3okKCFDy9WwferkHjtxib7t3xIUQtHxnIwtx4mpg26/HfwVNVDb4oI9RHmx5WGelRVlrtiw43zboCLaxv46AZeB3IlTkwouebTr1y2NjSpHz68WNFjHvupy3q8TFn3Hos2IAk4Ju5dCo8B3wP7VPr/FGaKiG+T+v+TQqIrOqMTL1VdWV1DdmcbO8KXBz6esmYWYKPwDL5b5FA1a0hwapHiom0r/cKaoqr+27/XcrS5UwSMbQAAAABJRU5ErkJggg==)](https://deepwiki.com/Latte72R/LaCC) [![GitHub License](https://img.shields.io/badge/License-MIT-green.svg?logo=data:image/png;base64,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)](https://github.com/Latte72R/LaCC/blob/main/LICENSE)

## Supported Features

### 1. Data Types

- **Primitive**: `int`, `char`, `void`  
- **Derived**: pointer types (`T*`), arrays (`T[]`)  
- **Composite**: structures (`struct`), unions (`union`), enumerations (`enum`)  

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
    You can omit any or all of the three components of a for loop          (initialization, condition, and step).
  - `while (condition) { … }`  
  - `do { … } while (condition);` 
- **Loop Control**  
  - `break` exits a loop  
  - `continue` skips to the next iteration

### 5. Operators

* **Arithmetic**: `+`, `-`, `*`, `/`, `%`
* **Relational**: `==`, `!=`, `<`, `<=`, `>`, `>=`
* **Logical**: `&&`, `||`, `!`
* **Bitwise**: `&`, `|`, `^`, `~`, `<<`, `>>`

### 6. Others

- **Include directive** (with limitations)  
  LaCC can process `#include` statements with double quotes like `#include "lacc.h"`, 
  but it does not support the standard library headers like `<stdio.h>` in the traditional sense.

- **Initializer lists for arrays** (with limitations)  
  1. **Array initialization with a list of values:**  
     `int arr[3] = {3, 6, 2};`
  2. **String literal initialization for character arrays:**  
     `char str[15] = "Hello, World!\n";`  
  
  The initializer list can **only** contain numeric literals, 
  and there's **no** support for nested initializer lists.

- **Extern declarations**  
  LaCC supports external variable declarations with basic types, pointers, and arrays.

- **Typedef support**  
  LaCC supports the `typedef` keyword for creating type aliases.

- **Type qualifiers & storage-class specifiers:** `const`, `volatile`, `static`

- **`goto` statement and labels**  
  LaCC supports `goto` statements and label definitions, allowing for non-linear control flow.

- **Struct and Union member access**  
  Both dot notation (`.`) for direct access and arrow notation (`->`) for pointer access are supported.

- **Binary and Hexadecimal numbers:** `0b001011` or `0xFF2A`

- **Comment support**
  ```c
  // single-line comment
  /* multi-line
     comment */
  ```

- **`switch` statements and `case` / `default` labels**  
  LaCC supports `switch` statements with `case` labels for branching based on the value of an expression.

- **Explicit type casting**  
  LaCC allows explicit type casting between compatible pointer types.

## Unsupported Constructs

LaCC does **not** support the following:
 
- Ternary conditional operator (`?:`)  
- Extended primitive types: `unsigned`, `long`, `float`, `double`, etc.  
- Type qualifiers & storage-class specifiers: `register`, `auto`, etc.  
- No initializer lists for structs (e.g.,  `struct AB p = {.a = 1, .b = 2};`)  
- Inline assembly  
- Preprocessor directives: `#define`, `#ifdef`, etc.  
- Variadic functions (functions with `...` parameters)  
- The comma operator
- Function pointers
- Nested functions (functions defined within other functions) 
- Variable Length Arrays (VLAs)


## Limitations

### Single‐Unit Compilation
LaCC only handles one .c file at a time — there's no support for separate compilation or linking multiple translation units.

### No Optimizations
There are no code-generation optimizations beyond what's needed to make it work.


## Getting Started with LaCC

### 1. Clone the repository and enter it

  ```bash
  git clone https://github.com/Latte72R/LaCC
  cd LaCC
  ```

  After that, you have a few `make` targets to build and test your compiler:

### 2. Build the self-hosted compiler

  ```bash
  make selfhost
  ```

  Here, a bootstrap compiler `bootstrap` is used to recompile the compiler source itself, 
  producing a self-hosted compiler named `lacc`.  
  This ensures that your compiler can correctly compile its own code.  

### 3. Run specific files with the self-hosted compiler

  ```bash
  make run FILE=./examples/lifegame.c
  make run FILE=./examples/rotate.c
  ```

  This command compiles and runs the specified C file using the self-hosted compiler `lacc`.

### 4. Run unit tests with the self-hosted compiler

  ```bash
  make unittest
  ```

  Passing all tests confirms that your self-hosted compiler behaves as expected.  
  The unit tests are located in the `tests/unittest.c` file.  

### 5. Run warning tests with the self-hosted compiler

  ```bash
  make warntest
  ```

  This command runs warning tests to ensure that the compiler correctly identifies and reports warnings.
  The warning tests are located in the `tests/warntest.c` file.

### 6. Run error tests with the self-hosted compiler

  ```bash
  make errortest
  ```

  This command runs error tests to ensure that the compiler correctly identifies and reports errors.
  The error tests are located in the `tests/errortest.sh` file.  

### 7. Clean up build artifacts

  ```bash
  make clean
  ```

  Removes the generated binaries and assembly files created during the build process.

### 7. Show help

  ```bash
  make help
  ```

  Displays a list of available make targets and their descriptions.


## About the Author  
LaCC is designed and maintained by student engineer **Latte72** !  

### Links  
- **Website:** [https://latte72.net/](https://latte72.net/)
- **GitHub:** [@Latte72R](https://github.com/Latte72R/LaCC)
- **X a.k.a Twitter:** [@Latte72R](https://twitter.com/Latte72R)
- **Qiita:** [@Latte72R](https://qiita.com/Latte72R)
