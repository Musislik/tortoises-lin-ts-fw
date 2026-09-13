# Embedded C Coding Style Guide

## 1. Introduction
This document defines the coding standards and best practices for the project. The primary goal is to maintain a consistent, readable, and safe codebase. In embedded systems, strict adherence to these rules helps prevent memory bugs, undefined behavior, and concurrency issues.

## 2. Naming Conventions
* **Variables:** `camelCase` (e.g., `sensorValue`, `rxBuffer`).
* **Functions:** `camelCase` (e.g., `calculateTemperature`, `initHardware`).
* **Types & Structs:** `PascalCase` with a `_t` suffix (e.g., `ConfigBlock_t`, `NodeState_t`).
* **Macros & Constants:** `UPPER_SNAKE_CASE` (e.g., `MAX_BUFFER_SIZE`, `PID_GET_TEMP`).
* **Global Variables:** Prefix with `g` (e.g., `gActiveConfig`, `gIsInitialized`).
* **Pointers:** Prefix with `p` to denote a pointer (e.g., `pBuffer`, `pConfig`).
* **Files:** `camelCase.c` and `camelCase.h` (e.g., `linDriver.c`, `sensorConfig.h`).

## 3. Formatting & Indentation
* **Indentation:** Use 4 spaces. **No tabs.**
* **Line Length:** Maximum of 100 characters per line.
* **Braces:** Use the "One True Brace Style" (1TBS) / K&R style. Opening brace on the same line, closing brace on a new line.
  ```c
  if (condition) {
      // code
  } else {
      // code
  }
  ```
* **Spacing:** Add a single space after keywords (`if`, `for`, `while`, `switch`) and around binary operators (`+`, `-`, `=`, `==`, `<`). Do not put a space after a function name when calling it.

## 4. Data Types
* Always use fixed-width integer types from `<stdint.h>` (e.g., `uint8_t`, `int32_t`, `uint16_t`).
* Never use plain `int`, `long`, or `char` (unless directly interfacing with standard C string functions).
* Use `<stdbool.h>` (`bool`, `true`, `false`) for logical and boolean states.

## 5. Control Structures
* **Braces are mandatory** for all `if`, `else`, `while`, and `for` statements, even if the block contains only a single line of code.
  ```c
  // BAD
  if (hasError) return false;

  // GOOD
  if (hasError) {
      return false;
  }
  ```
* Always include a `default` case in `switch` statements to handle unexpected values.

## 6. Variables and Scope
* Declare variables as close to their first use as possible (C99 style).
* Always initialize variables upon declaration to prevent undefined behavior.
* Limit the scope of variables. Use the `static` keyword for module-level variables and helper functions to hide them from the global namespace.

## 7. Functions
* Functions should be short, focused, and do exactly one thing (Single Responsibility Principle).
* Pass large data structures (like configuration structs) by pointer. Use a `const` pointer if the function should not modify the data (e.g., `void printConfig(const ConfigBlock_t *config)`).
* Always check the return values of functions that can fail.

## 8. Source and Header Files (.c / .h)
* **Header Files (`.h`):** 
  * Contain **only public interfaces**: function prototypes, public macros, public `typedef`s (structs/enums), and `extern` variable declarations.
  * Never put function implementations (except `static inline`) or variable definitions (e.g., `int myVar;`) in header files.
  * All header files must have include guards using `#ifndef`, `#define`, and `#endif`.
    ```c
    #ifndef MODULE_NAME_H
    #define MODULE_NAME_H
    
    // public declarations
    
    #endif // MODULE_NAME_H
    ```
* **Source Files (`.c`):**
  * Contain the actual implementations.
  * Private module functions and module-level variables **must be declared `static`** to keep them private to the file.
  * Private structs, enums, macros, and **constants** (e.g., `#define` or `static const` variables) must be defined in the `.c` file if they are not used outside the module. This prevents namespace pollution and unintended dependencies.
  * **Encapsulation & Opaque Pointers:** Hide internal struct details (variables) in the `.c` file whenever possible. In the `.h` file, use forward declarations (e.g., `typedef struct Module_s* Module_Handle;`). This strictly separates the interface from the implementation, prevents external manipulation of internal state, and reduces recompilation dependencies.
* **Include Rules (`#include`):**
  * Use angle brackets `< >` for standard libraries and vendor SDKs (e.g., `<stdint.h>`, `<ti/devices/...>`).
  * Use quotes `" "` for local project headers (e.g., `"linDriver.h"`).
  * **Include Order in `.c` files:**
    1. The module's own header (e.g., `linDriver.c` includes `"linDriver.h"` first). This proves the header is self-contained.
    2. Standard C library headers (`<stdint.h>`, `<stdbool.h>`).
    3. Vendor / external SDK headers.
    4. Other local project headers.
  * **Minimize Includes in `.h` files:** A header should `#include` only what it absolutely needs to compile. Use forward declarations when you only need a pointer to a struct.

## 9. Comments and Documentation
* Write self-documenting code. Use descriptive names for functions and variables so that fewer comments are needed.
* **Comments should explain *WHY*, not *WHAT*.** The code itself already shows what is happening.
* Use Doxygen-style comments (`/** ... */`) for documenting public APIs (functions, structs, and macros exported in header files).

## 10. Embedded & Safety Rules
* **No Magic Numbers:** Never hardcode numeric values in the logic. Use `#define` or `enum` for constants.
* **Const Correctness:** Use the `const` keyword extensively for read-only variables, lookup tables, and function parameters. This places data in Flash instead of RAM and prevents accidental modification.
* **No Dynamic Memory:** Avoid `malloc()`, `calloc()`, and `free()`. Use static allocation to prevent memory leaks, fragmentation, and unpredictable execution times.
* **Interrupt Service Routines (ISRs):** Keep ISRs extremely short and fast. Set a flag (e.g., `volatile bool dataReady = true;`) and defer heavy processing to the main application loop.
* **Volatile Keyword:** Always use the `volatile` keyword for variables modified inside an ISR and read in the main loop, or when mapping hardware registers.
