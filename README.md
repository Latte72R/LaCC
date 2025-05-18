![](./LaCC.png)

LaCC is a minimalist C compiler that implements only the core language features you need to get simple C programs running.

[![DeepWiki](https://img.shields.io/badge/DeepWiki-Latte72R%2FLaCC-blue.svg?logo=data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACwAAAAyCAYAAAAnWDnqAAAAAXNSR0IArs4c6QAAA05JREFUaEPtmUtyEzEQhtWTQyQLHNak2AB7ZnyXZMEjXMGeK/AIi+QuHrMnbChYY7MIh8g01fJoopFb0uhhEqqcbWTp06/uv1saEDv4O3n3dV60RfP947Mm9/SQc0ICFQgzfc4CYZoTPAswgSJCCUJUnAAoRHOAUOcATwbmVLWdGoH//PB8mnKqScAhsD0kYP3j/Yt5LPQe2KvcXmGvRHcDnpxfL2zOYJ1mFwrryWTz0advv1Ut4CJgf5uhDuDj5eUcAUoahrdY/56ebRWeraTjMt/00Sh3UDtjgHtQNHwcRGOC98BJEAEymycmYcWwOprTgcB6VZ5JK5TAJ+fXGLBm3FDAmn6oPPjR4rKCAoJCal2eAiQp2x0vxTPB3ALO2CRkwmDy5WohzBDwSEFKRwPbknEggCPB/imwrycgxX2NzoMCHhPkDwqYMr9tRcP5qNrMZHkVnOjRMWwLCcr8ohBVb1OMjxLwGCvjTikrsBOiA6fNyCrm8V1rP93iVPpwaE+gO0SsWmPiXB+jikdf6SizrT5qKasx5j8ABbHpFTx+vFXp9EnYQmLx02h1QTTrl6eDqxLnGjporxl3NL3agEvXdT0WmEost648sQOYAeJS9Q7bfUVoMGnjo4AZdUMQku50McDcMWcBPvr0SzbTAFDfvJqwLzgxwATnCgnp4wDl6Aa+Ax283gghmj+vj7feE2KBBRMW3FzOpLOADl0Isb5587h/U4gGvkt5v60Z1VLG8BhYjbzRwyQZemwAd6cCR5/XFWLYZRIMpX39AR0tjaGGiGzLVyhse5C9RKC6ai42ppWPKiBagOvaYk8lO7DajerabOZP46Lby5wKjw1HCRx7p9sVMOWGzb/vA1hwiWc6jm3MvQDTogQkiqIhJV0nBQBTU+3okKCFDy9WwferkHjtxib7t3xIUQtHxnIwtx4mpg26/HfwVNVDb4oI9RHmx5WGelRVlrtiw43zboCLaxv46AZeB3IlTkwouebTr1y2NjSpHz68WNFjHvupy3q8TFn3Hos2IAk4Ju5dCo8B3wP7VPr/FGaKiG+T+v+TQqIrOqMTL1VdWV1DdmcbO8KXBz6esmYWYKPwDL5b5FA1a0hwapHiom0r/cKaoqr+27/XcrS5UwSMbQAAAABJRU5ErkJggg==)](https://deepwiki.com/Latte72R/LaCC)

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
  - `do { … } while (condition);` 
- **Loop Control**  
  - `break` exits a loop  
  - `continue` skips to the next iteration

### 5. Operators

* **Arithmetic**: `+`, `-`, `*`, `/`, `%`
* **Relational**: `==`, `!=`, `<`, `<=`, `>`, `>=`
* **Logical**: `&&`, `||`, `!`
* **Bitwise**: `&`, `|`, `^`, `~`, `<<`, `>>`

## Unsupported Constructs

LaCC does **not** support the following:

- Nested functions (functions defined within other functions)  
- `switch` statements and `case`/`default` labels  
- `goto` statement and labels
- Ternary conditional operator (`?:`)  
- `union` types  
- Extended primitive types: `unsigned`, `long`, `float`, `double`, etc.  
- Type qualifiers & storage-class specifiers: `const`, `volatile`, `static`, `register`, `auto`, etc.  
- No initializer lists for structs (e.g.,  `struct AB p = {.a = 1, .b = 2};`)  
- Inline assembly  
- Preprocessor directives: `#include`, `#define`, `#ifdef`, etc.  
- Variadic functions (functions with `...` parameters)  
- The comma operator
- Function pointers
- Explicit type casting


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

### 2. Build the compiler

  ```bash
  make
  ```

  This invokes the default target, compiling all sources into the `lacc` executable.  
  If nothing has changed, you'll see

  ```bash
  'lacc' is up to date.
  ```

### 3. Build the self-hosted compiler

  ```bash
  make selfhost
  ```

  Here, `lacc` is used to recompile the compiler source itself, producing a second-stage compiler named `laccs`. 
  This ensures that your compiler can correctly compile its own code.

### 4. Run primary tests

  ```bash
  make test
  ```

  Similarly, this runs

  ```bash
  ./multitest.sh ./lacc
  ```

  to validate the first-stage compiler `lacc` against the same test suite.

### 5. Run self-host tests

  ```bash
  make selfhost-test
  ```

  Internally this runs

  ```bash
  ./multitest.sh ./laccs
  ```

  and executes a suite of example programs (unit tests, `prime.c`, `fizzbuzz.c`, etc.) using `laccs`. 
  Passing all tests confirms that your self-hosted compiler behaves as expected.

### 6. Clean up build artifacts

  ```bash
  make clean
  ```

  Removes the `lacc` and `laccs` binaries, object files (`*.o`), backup files (`*~`) and any temporary files (`tmp*`), letting you start from a clean slate.


## About the Author  
LaCC is designed and maintained by student engineer **Latte72**!  

### Links  
- **Website:** [https://latte72.net/](https://latte72.net/)
- **GitHub:** [@Latte72R](https://github.com/Latte72R/LaCC)
- **X a.k.a Twitter:** [@Latte72R](https://twitter.com/Latte72R)
- **Qiita:** [@Latte72R](https://qiita.com/Latte72R)
