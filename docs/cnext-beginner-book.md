<!-- Cnext: The Complete Beginner's Guide -->

# Cnext: The Complete Beginner's Guide

*From your very first program to real applications — a friendly, practical
introduction to Cnext, a modern language that compiles to C.*

> Built with **definitions** (what each thing *is*), **explanations** (how it
> works), **theory** (why it works that way), and **real-world examples**
> (code you could actually ship). Every chapter follows the same rhythm so you
> always know what to expect.

---

## How to Use This Book

**Cnext** is a modern, high-level programming language that compiles down to
**C**. That one sentence is the reason this book exists: Cnext gives you a
clean, friendly syntax with modern features — and still lets you ship fast,
portable programs the way C programmers do.

### Who this book is for

- **Absolute beginners** who have never programmed. Start at Part I and go in
  order — every new idea builds on the ones before it.
- **Students** learning a second (or third) language. Skim the fundamentals and
  spend time on Parts IV and V.
- **Working developers** evaluating Cnext. Read Part I for the pitch, then jump
  to the chapters that interest you.

### How the book is organized

The book is split into five parts. Each chapter sticks to the same rhythm:

1. **Definition** — a plain-language answer to *what is this?*
2. **Explanation** — how it works, with small, runnable examples.
3. **Theory** — the computer-science idea underneath, so you understand *why*.
4. **Real-world example** — code that solves a realistic problem.
5. **Exercise** — a small task to stretch your understanding.

### Callout boxes you will see

| Box | Meaning |
|-----|---------|
| **Key point** | The single idea to remember from that section. |
| **Theory** | The computer science behind a feature. |
| **Real world** | Where you meet this in actual software. |
| **Tip** | Practical guidance. |
| **Warning** | Common pitfalls to avoid. |
| **Exercise** | A task to try yourself. |

### Tips for learning placement

- **Read a little, type a lot, break things.** Typing code by hand (not
  copy-pasting) and intentionally breaking it is the fastest way to learn.
- **Run every example.** Don't trust the book — verify with your computer.
- **Keep a "snippet box."** Save the tiny examples you like; you'll reuse them
  for your own projects.

---

## Table of Contents

**PART I — Getting Started**
1. What Is Cnext? (and Why C?)
2. Installing Cnext
3. Your First Program
4. The Basics: Comments, Variables, Input & Output

**PART II — Language Fundamentals**
5. Types & Data Types
6. Operators: Doing Math and Comparisons
7. Making Decisions with `if` and `match`
8. Loops: `while` and `for`
9. Arrays & Slices
10. Strings
11. Functions
12. Handling Errors: `throw` and `try`/`catch`
13. Nullability: Saying "I Don't Know"

**PART III — Objects & Data**
14. Structs, Enums & Pattern Matching
15. Classes & Object-Oriented Programming
16. Generics: Code That Works with Any Type

**PART IV — Modern Features**
17. Closures & Higher-Order Functions
18. Generators & Coroutines
19. Async: Doing I/O Without Blocking
20. Threading: Doing Two Things at Once

**PART V — The Professional Toolkit**
21. The Tools: run, build, fmt, lint, repl, test, doctor
22. The Standard Library & Global Built-ins
23. Projects, Packages & Real Software
24. Capstone: Build a Real Mini-Project

**Reference & Appendices**
- Appendix A — Glossary
- Appendix B — Troubleshooting
- Appendix C — Quick Keyword & Operator Reference
- Appendix D — Your Next Steps

---
# PART I — Getting Started

> Set up the language, run your first program, and learn the core vocabulary
> of programming.

---

## 1. What Is Cnext? (and Why C?)

> **Difficulty: BEGINNER**

### 1.1 Definition

- **Cnext** is a **high-level**, **statically typed**, **compiled** programming
  language that **transpiles to C**. You write code in a plain-text file with a
  `.cn` extension; the Cnext compiler turns it into portable C source code, and
  a standard C compiler (GCC or Clang) turns that into a fast, native
  executable for your machine.

### 1.2 Explanation — how computers "speak"

When you want a computer to do something, you must communicate in a form it can
execute. At the very bottom, a CPU understands only small, primitive
instructions — *move this number*, *compare two values*, *jump to another
place in the program*. These are called **machine code**, and they differ
between processor families (Intel, ARM, and so on).

Nobody wants to write machine code by hand. So we write in a language closer to
how humans think, and a tool translates it for us.

```
your code (hello.cn)  ──compiler──▶  executable  ──run──▶  output
```

> **Theory — high-level vs. low-level languages**
>
> A **high-level language** (Python, JavaScript, Cnext) is written in words and
> symbols close to what people mean: `if x > 0`. A **low-level language**
> (assembly, machine code) is written in the exact, tiny steps a processor
> executes. High-level languages are easier and safer to write; the trade-off
> is that a **compiler** must translate them for you.

### 1.3 How Cnext works — the journey of a file

Follow one file through the compiler's pipeline:

```
your_program.cn ──(cnext build)──▶ C source ──(gcc/clang)──▶ your_program
```

1. **Lexing** — your text becomes meaningful *tokens* (`main`, `{`,
   `printin`, ...).
2. **Parsing** — tokens are assembled into a tree that matches the language's
   grammar.
3. **Semantic analysis** — the compiler checks that types match, names exist,
   and the rules are respected.
4. **Code generation** — the validated tree is turned into C source code.
5. **Native compilation** — GCC or Clang builds a native executable.

> **Key point**
>
> "Cnext" means **C NEXT**: modern syntax on top of C's proven, portable, and
> fast foundation. The generated output is ordinary C understood by any
> mainstream toolchain.

### 1.4 Why build on C?

C has written operating systems, databases, game engines, and much of the
world's infrastructure. By generating C, Cnext inherits three things
automatically:

- **Portability** — any platform with a C compiler can run Cnext programs.
- **Performance** — near-native speed without hand-writing assembly.
- **Interoperability** — your Cnext programs can call the enormous universe of
  C libraries.

### 1.5 What Cnext looks like

Here is a complete program you will dissect in Chapter 3:

```cnext
func greet(str name): str {
    return "Hello, " + name + "!"
}

main {
    printin(greet("World"))
}
```

Notice there is little ceremony: `func` declares a function, the return type
follows a colon, and `main` is where every program starts. Semicolons are
optional.

> **Real world**
>
> The exact pipeline you are learning here — *source → C → executable* — powers
> the Linux kernel, Python, and Git. Understanding this skill will serve you
> for your entire career.

> **Exercise 1.1**
>
> Without running any code, predict what `printin(2 + 3 * 4)` will print. Then
> look at Chapter 6 to confirm Cnext follows the usual "multiplication before
> addition" rule.

---
## 2. Installing Cnext

> **Difficulty: BEGINNER**

### 2.1 Definition

- **Installing Cnext** means putting the `cnext` command on your computer's
  **PATH** so that typing `cnext` in a terminal works from any folder.

### 2.2 What you'll need

Cnext compiles your code to C, so it needs a C compiler to finish the job:

| Tool | Why you need it |
|------|-----------------|
| A text editor | To write code (VS Code, Notepad++, or any editor). |
| **GCC** or **Clang** | The C compiler that turns Cnext's C output into an executable. |
| Make (optional) | Used by the build process when installing from source. |
| Git (optional) | To clone the repository. |

- On **Linux**, GCC is usually already installed (`sudo apt install gcc` on
  Debian/Ubuntu).
- On **Windows**, install **MinGW (MSYS2/UCRT64)** or another toolchain that
  provides `gcc`.
- On **macOS**, install **Xcode Command Line Tools** (`xcode-select --install`).

### 2.3 Install Cnext from source

```bash
git clone https://github.com/Melton122/cnext.git
cd cnext
make
```

This builds the `cnext` executable in the project root. You can also download a
pre-built package for Windows (for example `cnext-windows-x64-10.0.0.zip`).

> **Tip — pip / package helpers**
>
> Later releases may offer installers (`install.sh`, `install.ps1`,
> `install.bat`). Check the project's README for the current recommended way.

### 2.4 Put `cnext` on your PATH

So you can type `cnext` from any folder:

- **Linux/macOS:**
  ```bash
  export PATH=$PATH:/path/to/cnext
  ```
- **Windows:** add the folder containing `cnext.exe` to your system PATH, or run
  the included `install.bat` / `install.ps1`.

### 2.5 Check that it works

```bash
cnext help
```

You should see a list of commands (`run`, `build`, `fmt`, `repl`, `test`,
`doctor`, and more). If you see `command not found` or `not recognized`, the
`cnext` executable is not on your PATH yet — see Appendix B.

> **Theory — what a "terminal" and "PATH" are**
>
> A **terminal** is a text-based window where you type commands. The **PATH**
> is a list of folders your operating system searches whenever you type a
> command name. Putting a program on the PATH means "the OS can find it from
> anywhere." This is how all command-line tools work on every operating system.

> **Real world**
>
> Tools like `git`, `python`, and `node` all follow this same pattern: install
> once, put the executable on your PATH, and then invoke it by name. Learning
> how PATH works once will apply to every other tool you ever use.

> **Exercise 2.1**
>
> Run `cnext version`. Write down the version number — you'll want it handy when
> you ask for help or report a bug later.

---
## 3. Your First Program

> **Difficulty: BEGINNER**

### 3.1 Definition

- A **program** is a set of instructions a computer follows, stored in a text
  file. Cnext programs use the `.cn` extension.

### 3.2 Hello, World!

Create a file named `hello.cn` with exactly this content:

```cnext
// The classic first program
main {
    printin("Hello, World!")
}
```

Run it from a terminal in the same folder:

```bash
cnext run hello.cn
```

You should see:

```
Hello, World!
```

> **Key point**
>
> `main { ... }` is where every standalone program begins. Everything between
> the braces runs, top to bottom, when the program starts.

### 3.3 Understand every line

| Piece | Meaning |
|-------|---------|
| `// ...` | A **comment**. The compiler ignores it; it's a note for humans. |
| `main` | The entry-point function, run automatically. |
| `{ ... }` | A **block** — groups statements together. |
| `printin(...)` | A built-in function that prints text and then a new line. |
| `"Hello, World!"` | A **string literal** — a sequence of characters. |

> **Theory — what "running" a program means**
>
> When you type `cnext run hello.cn`, the compiler does the five jobs from
> Chapter 1 (lex, parse, analyze, generate C, compile to native) and then
> executes the result. Your operating system loads the program into memory,
> hands it to the CPU, and the instructions in `main` run in order — exactly
> once.

### 3.4 Create a real project folder

Instead of a loose file, scaffold a project:

```bash
cnext new my_first_app
cd my_first_app
cnext run src/main.cn
```

This creates a tidy layout — source code in `src/`, a `cnext.toml`
configuration file, and space for packages and tests. Professional codebases
organize files like this from day one.

### 3.5 `run` vs. `build`

- `cnext run file.cn` — **compile and immediately run** your program. Perfect
  for iterating while you learn.
- `cnext build file.cn` — **compile only**, producing a standalone executable
  you can ship or run later. This is what release software does.

```bash
cnext run hello.cn       # compile + run now
cnext build hello.cn      # compile only, produce an executable
./hello                   # run the produced executable
```
### 3.6 The REPL: try before you write files

A **REPL** is an interactive prompt that *reads* your input, *evaluates* it,
and *prints* the result — great for experimenting:

```
cnext repl
> printin("testing")
testing
> 1 + 1
2
> exit
```

> **Real world**
>
> Every serious language ships both a "build once, run anywhere" path for
> production and a fast feedback loop for development. Cnext's REPL is that
> feedback loop; the compiler producing an executable is the production path.

> **Exercise 3.1**
>
> Edit `hello.cn` so it also prints `Welcome to Cnext!` on a second line. Then
> build it with `cnext build` and verify the executable prints both lines.

<details><summary>Suggested solution</summary>

```cnext
main {
    printin("Hello, World!")
    printin("Welcome to Cnext!")
}
```

</details>

---

## 4. The Basics: Comments, Variables, Input & Output

> **Difficulty: BEGINNER**

### 4.1 Definition

Every program is built from a small set of ideas: named containers
(**variables**), ways to put data in and get data out (**input/output**), and
rules for structuring files (**comments**, **blocks**). This chapter covers
them all.

### 4.2 Comments — notes for humans

A **comment** is text in your code that the compiler ignores. There are two
styles:

```cnext
// This is a single-line comment.

/* This is a
   multi-line comment. */
main {
    // printin("this is commented out and will not run")
    printin("this runs")
}
```

> **Tip**
>
> Comments explain *why*, not *what*. Good code is often self-explanatory;
> comments shine when they capture intent (for example, *"we clamp here so the
> value never exceeds 100"*) or non-obvious trade-offs.

### 4.3 Variables

A **variable** is a named box that holds a value. You can declare it with an
explicit type, or let the compiler infer the type with `var`:

```cnext
int score = 90            // explicit type: 32-bit whole number
var name = "Ada"          // inferred type: str
var pi = 3.14             // inferred: float
var passed = true         // inferred: bool
score = 95                // values can change later
```

> **Key point**
>
> `var x = value` lets Cnext figure out the type for you (**type inference**).
> It is still *statically typed*: the type is fixed at compile time, so you
> can't later store a string in a variable that was created as an integer.
### 4.4 Constants

When a value should never change, declare it with `const`:

```cnext
const int MAX_RETRIES = 3
const str APP_NAME = "Notes"
// MAX_RETRIES = 5   // compile error: cannot reassign a constant
```

### 4.5 Blocks & scope

Every `{ ... }` opens a new **scope**. A variable declared inside a block
exists only until the block ends:

```cnext
main {
    var outside = 10
    if true {
        var inside = 20   // exists only in this block
        printin(outside)  // ok: outside is visible here
    }
    // printin(inside)    // error: `inside` is gone
}
```

### 4.6 Printing output

Use `print` (no newline) and `printin` (prints a new line after):

```cnext
printin("line 1")
printin("line 2")
print("no newline here -> ")
printin("continues on same line")
```

Output:

```
line 1
line 2
no newline here -> continues on same line
```

### 4.7 Reading input

Ask the user for text with `input` (or `input("prompt: ")`):

```cnext
main {
    print("Enter your name: ")
    var name = input()
    printin("Hello, " + name + "!")
}
```

> **Real world**
>
> Separating *input*, *processing*, and *output* is the oldest software
> pattern. Programs like a grade calculator read data, compute, and write
> results — exactly what this tiny greeting program does with text.

> **Exercise 4.1**
>
> Write a program that asks for a user's name and favorite number, then prints
> a sentence such as `Hello, Ada! 7 is a great choice.` Use variables and
> string concatenation.

<details><summary>Suggested solution</summary>

```cnext
main {
    print("Name: ")
    var name = input()
    print("Favorite number: ")
    var num = input()
    printin("Hello, " + name + "! " + num + " is a great choice.")
}
```

</details>

---
# PART II — Language Fundamentals

> Types, operators, control flow, functions, and collections — the toolkit of
> every program.

---

## 5. Types & Data Types

> **Difficulty: BEGINNER**

### 5.1 Definition

- A **type** describes what kind of value a variable holds and what you can do
  with it — for example, whether it's a whole number, a decimal, text, or a
  true/false flag. Cnext is **statically typed**, meaning every variable's type
  is known *at compile time*.

### 5.2 The primitive types

| Type | Kind | Example |
|------|------|---------|
| `int` | 32-bit whole number | `42`, `-10` |
| `long` | 64-bit whole number (bigger range) | `9999999999` |
| `float` | 32-bit decimal | `3.14` |
| `double` | 64-bit decimal (more precise) | `3.14159265358979` |
| `str` | Text (a string) | `"hello"` |
| `bool` | True or false | `true`, `false` |
| `char` | A single character (single quotes!) | `'a'`, `'\n'` |
| `byte` | 8-bit unsigned integer (0–255) | `255` |
| `uint` | 32-bit unsigned integer | `4294967295` |
| `ulong` | 64-bit unsigned integer | `18446744073709551615` |
| `ushort` | 16-bit unsigned integer | `65535` |
| `ubyte` | 8-bit unsigned integer | `255` |

```cnext
int age = 25
float price = 9.99
str name = "Cnext"
bool is_fun = true
char letter = 'Q'

var anything = 100        // var: Cnext infers this is an int
var also_text = "Hi"      // var: inferred as str
```

> **Theory — "signed" vs. "unsigned" and why sizes matter**
>
> A computer stores values in fixed-size chunks of **bits**. A 32-bit integer
> uses 32 bits, so it can represent 2³² different values — roughly 4.3
> billion. **Signed** types split that range between negative and positive
> (about −2.1 billion to +2.1 billion); **unsigned** types use the whole range
> for positive numbers. Bigger sizes hold bigger numbers but use more memory.
> Choosing the right size is a classic performance trick in systems
> programming.

> **Key point**
>
> **Strings** use double quotes `"..."`. **Characters** use single quotes
> `'...'`. Mixing them up — `'hello'` or `"a"` — is one of the most common
> beginner mistakes.

### 5.3 Type inference with `var`

`var` asks Cnext to figure out the type from the value. It's still statically
typed — the type is fixed once inferred.

```cnext
var x = 42          // int
var pi = 3.14       // float
var name = "Ada"    // str
var flag = true     // bool
```

### 5.4 Type aliases

You can give a type a more descriptive name with `type`:

```cnext
type Name = str
type Age = int

Name person = "Ada"
Age years = 36
```

### 5.5 Converting between types (`as`)

Use `as` to convert a value from one type to another:

```cnext
float f = 7.8
int i = f as int       // 7  (decimal part is dropped)
int n = 5
float f2 = n as float  // 5.0
```

> **Real world**
>
> Choosing the right type is everywhere in software. An online store stores
> prices as `float`/`double`, user IDs as `long`, and a "is the order shipped?"
> flag as `bool`. A game stores a health counter as `int` and a player's speed
> as `float`. Getting types right prevents the #1 class of real bugs.

> **Exercise 5.1**
>
> Create variables for: your age (`int`), your height in meters (`float`), the
> first letter of your first name (`char`), and whether you've written a program
> before (`bool`). Print them all.

---
## 6. Operators: Doing Math and Comparisons

> **Difficulty: BEGINNER**

### 6.1 Definition

- An **operator** is a symbol that performs a calculation on one or more
  values. The values an operator works on are called **operands**.

### 6.2 Arithmetic operators

```cnext
var a = 10 + 5   // 15  addition
var b = 10 - 5   // 5   subtraction
var c = 10 * 5   // 50  multiplication
var d = 10 / 5   // 2   division
var e = 10 % 3   // 1   remainder (modulo)
```

- `+` adds, `-` subtracts, `*` multiplies, `/` divides, and `%` gives the
  **remainder** of a division (great for "is it even?" checks).

> **Theory — operator precedence**
>
> Just like in school, some operations happen before others. In Cnext (as in
> almost every language), **multiplication, division, and modulo** bind tighter
> than **addition and subtraction**. So `2 + 3 * 4` is `2 + (3 * 4) = 14`, not
> `(2 + 3) * 4`. Use parentheses `( ... )` to force your intended order.

### 6.3 Comparison operators

Comparisons always produce a `bool` (`true` or `false`):

```cnext
10 == 10   // true   equal to
10 != 5    // true   not equal to
10 > 5     // true   greater than
10 < 5     // false  less than
10 >= 10   // true   greater than or equal
10 <= 5    // false  less than or equal
```

### 6.4 Logical operators

Combine conditions into one `true`/`false` answer:

```cnext
true && false   // false  AND: both must be true
true || false   // true   OR: at least one must be true
!true           // false  NOT: flips true to false
```

### 6.5 Assignment & compound assignment

```cnext
var x = 10
x = 20      // assign a new value
x += 5      // x = x + 5  → 25
x -= 3      // x = x - 3  → 22
x *= 2      // x = x * 2  → 44
x /= 4      // x = x / 4  → 11
```

`+=`, `-=`, `*=` and `/=` are **shorthand** that say "take x, do this math, and
store the result back into x." They're exactly equivalent to the longer form.

### 6.6 Increment shorthand

```cnext
var n = 0
n = n + 1   // long form
n += 1      // shorter
```

### 6.7 String operators

The `+` operator also **concatenates** (joins) text:

```cnext
str a = "hello"
str b = " world"
str c = a + b          // "hello world"
bool same = (a == b)   // false
int len = a.length     // 5
```

### 6.8 Ternary operator

A short, single-line `if/else` that produces a value:

```cnext
var age = 18
str status = age >= 18 ? "adult" : "minor"
// status is "adult"
```

> **Real world**
>
> Operators are the workhorses of every real program. A shopping cart computes
> `total = total + itemPrice`; a thermostat checks `if (temp <= limit)`;
> a website validation checks `if (age < 13 || !accepted)`. Reading and writing
> these symbols fluently is a core skill.

> **Exercise 6.1**
>
> Predict the value of each, then check with a program:
> `20 / 4 + 2`, `20 - 4 * 2`, `(20 - 4) * 2`, `7 % 3`, and `5 > 3 && 2 > 3`.

---
## 7. Making Decisions with `if` and `match`

> **Difficulty: BEGINNER**

### 7.1 Definition

- **Control flow** is the order in which your program's statements run. The
  `if` statement and `match` expression let a program *choose* which code to
  run based on a **condition** (a `true`/`false` check).

### 7.2 The `if` statement

```cnext
var score = 75

if score >= 60 {
    printin("You passed!")
}
```

You can add more branches:

```cnext
var temp = 30

if temp > 35 {
    printin("Very hot")
} else if temp > 20 {
    printin("Warm")
} else {
    printin("Cool")
}
```

Cnext checks conditions **top to bottom** and runs the code for the *first*
one that is true. If none match, it runs the `else` block (if present).

> **Key point**
>
> The condition inside `if ( ... )` must be a `bool` — that is, something that
> is either `true` or `false`. `if score { }` won't compile if `score` is an
> `int`; you must write `if score > 0 { }`.

### 7.3 The `match` expression

When you want to compare one value against many possibilities — like an exact
choice — **`match`** is cleaner than a long chain of `if/else if`. A `match`
*produces a value*:

```cnext
enum Color {
    RED,
    GREEN,
    BLUE
}

func describe(Color c): str {
    match c {
        RED      => "Red as a rose"
        GREEN    => "Green as grass"
        BLUE     => "Blue as the sky"
        _        => "Some other color"
    }
}
```

The `_` is a **wildcard** that matches anything — it's `match`'s version of
`else`.

> **Theory — why `match` is better than `if` chains**
>
> When a value can take one of a few known choices, writing `if/else if` for
> each is repetitive and error-prone (you might miss a case). `match` forces
> you to list the cases in one place and makes the possible outcomes obvious at
> a glance. In compiler jargon this is called **exhaustive branching** — if a
> `match` is exhaustive, every possible input is handled.

> **Real world**
>
> `if`/`else` handles any *yes/no or range* question: `if balance < 0 { warn }`.
> `match` shines for *many named categories*: an HTTP status code (`200`, `404`,
> `500`), a weekday, a currency, a payment method. Choosing the right tool makes
> your code shorter and easier to follow.

> **Exercise 7.1**
>
> Write a program that reads an exam score (0–100) and prints a letter grade:
> `A` (90+), `B` (80–89), `C` (70–79), `D` (60–69), or `F` (below 60). Use
> `if`/`else if`.

<details><summary>Suggested solution</summary>

```cnext
main {
    var score = 84
    if score >= 90 {
        printin("A")
    } else if score >= 80 {
        printin("B")
    } else if score >= 70 {
        printin("C")
    } else if score >= 60 {
        printin("D")
    } else {
        printin("F")
    }
}
```

</details>

---
## 8. Loops: `while` and `for`

> **Difficulty: BEGINNER**

### 8.1 Definition

- A **loop** repeats a block of code. Loops are used whenever you need to do
  something many times — count, walk through a list, or wait for a condition.

### 8.2 The `while` loop

`while` repeats *as long as* its condition is true. The condition is checked
before each repetition:

```cnext
var i = 0
while i < 3 {
    printin("count: " + i)
    i = i + 1
}
```

Output:

```
count: 0
count: 1
count: 2
```

> **Warning — infinite loops**
>
> If you forget to update the counter (`i = i + 1`), the condition stays true
> forever and the program never stops. This is the **infinite loop** — one of
> the most common beginner bugs.

### 8.3 The `for` loop

`for` packs three things into one line: a start value, a keep-going condition,
and an update step:

```cnext
for int i = 0; i < 5; i = i + 1 {
    printin(i)
}
```

Read it as: "start with `i = 0`; keep going while `i < 5`; after each run do
`i = i + 1`." It prints `0, 1, 2, 3, 4`.

### 8.4 The `for-in` loop

`for ... in` walks through **each item** of a collection — no counting needed:

```cnext
int[] nums = {10, 20, 30}
for var n in nums {
    printin("next: " + n)
}
```

Output:

```
next: 10
next: 20
next: 30
```

### 8.5 `break` and `continue`

- **`break`** stops the loop entirely and jumps past it.
- **`continue`** skips the *rest of this one run* and moves to the next.

```cnext
for int i = 0; i < 10; i = i + 1 {
    if i == 3 {
        continue   // skip 3, keep going
    }
    if i == 6 {
        break      // stop the loop at 6
    }
    printin(i)     // prints 0,1,2,4,5
}
```

> **Theory — loops and the CPU**
>
> A loop doesn't physically repeat code on disk; the CPU runs the same
> instructions multiple times by *jumping back* to the start of the block each
> time the condition holds. This is why loops are sometimes called **iteration**
> or **looping constructs** — the machine is literally obeying "go back and do
> that again."

> **Real world**
>
> Loops drive almost every program. A weather app loops over hourly forecasts;
> an email client loops over messages; a bank loops over transactions to compute
> a total. Whenever you think "I have a lot of X to do," you're describing a
> loop.

> **Exercise 8.1**
>
> Use a `for` loop to print the even numbers from 0 to 20 (hint: check
> `i % 2 == 0`).

<details><summary>Suggested solution</summary>

```cnext
main {
    for int i = 0; i <= 20; i = i + 1 {
        if i % 2 == 0 {
            printin(i)
        }
    }
}
```

</details>

---
## 9. Arrays & Slices

> **Difficulty: BEGINNER**

### 9.1 Definition

- An **array** is an ordered list of values, all of the same type. You write it
  with curly braces: `{1, 2, 3}`. The type of an `int` array is written `int[]`.

### 9.2 Creating and reading arrays

```cnext
int[] nums = {1, 2, 3, 4, 5}
str[] words = {"hello", "world"}

printin(nums.length)  // 5   how many items
printin(nums[0])      // 1   first item
printin(nums[2])      // 3   third item
printin(nums[4])      // 5   last item
```

> **Key point — indexes start at 0**
>
> The first element is at index **0**, not 1. An array with 5 items has valid
> indexes `0, 1, 2, 3, 4`. Accessing `nums[5]` is **out of bounds** and will
> cause a runtime error.

### 9.3 Changing an element

```cnext
int[] nums = {1, 2, 3}
nums[1] = 99
printin(nums[1])   // 99
```

### 9.4 Looping over an array

Use `for-in` to visit each element:

```cnext
int[] nums = {10, 20, 30}
for var n in nums {
    printin("next: " + n)
}
```

### 9.5 Slices

A **slice** is a *view of part* of an array, created with `[start:end]`. The
end index is *not included*:

```cnext
int[] nums = {1, 2, 3, 4, 5}

int[] slice   = nums[1:3]   // {2, 3}
int[] fromTwo = nums[2:]    // {3, 4, 5}
int[] upToThree = nums[:3]  // {1, 2, 3}
```

> **Theory — arrays and memory**
>
> An array stores its values in **one continuous strip** of memory, one right
> after the other. That's why indexing is fast: to reach element `i`, the
> computer goes to the start of the strip and steps forward `i` slots. This
> layout is the reason arrays are the go-to structure for ordered data in
> systems programming.

> **Real world**
>
> Arrays hold the lists every app manages: a music playlist, a list of
> to-do items, the frames of a video, the pixels of an image. When a web page
> shows "5 new messages," a program almost certainly looped over an array of
> messages to count them.

> **Exercise 9.1**
>
> Create an array of 5 temperatures (`{18, 22, 30, 25, 20}`). Loop over it and
> print only the values that are 25 or higher.

<details><summary>Suggested solution</summary>

```cnext
main {
    int[] temps = {18, 22, 30, 25, 20}
    for var t in temps {
        if t >= 25 {
            printin(t)   // 30, 25
        }
    }
}
```

</details>

---
## 10. Strings

> **Difficulty: BEGINNER**

### 10.1 Definition

- A **string** (`str`) is text — a sequence of characters. In Cnext you write a
  string with double quotes: `"hello"`.

### 10.2 String basics

```cnext
str s1 = "hello"
str s2 = "world"

printin(s1.length)              // 5  number of characters
printin(s1 + " " + s2)          // "hello world"  (concatenation)
bool same = (s1 == s2)          // false
```

- `+` joins strings together (**concatenation**).
- `.length` gives the number of characters.

### 10.3 Interpolation

You can drop a variable's value *inside* a string using curly braces `{ }`.
This is called **interpolation** and it's cleaner than lots of `+`:

```cnext
str name = "Ada"
printin("Hello, {name}!")        // inserts the value of name
printin("2 + 3 = {2 + 3}")      // you can even put an expression
```

Output:

```
Hello, Ada!
2 + 3 = 5
```

> **Tip**
>
> Cnext interpolation uses `{name}`, *not* `$name`. If you write `$name`, the
> dollar sign prints literally.

### 10.4 Handy string helpers

Cnext ships many small string functions. You don't need to memorize them — just
know they exist and look them up (see the Standard Library chapter):

```cnext
str_upper("hello")               // "HELLO"
str_lower("WORLD")               // "world"
str_trim("  hi  ")               // "hi"
str_contains("hello", "ell")     // true
str_replace("hello", "l", "r")   // "herro"
str_substring("hello", 1, 3)     // "el"
str_repeat("ab", 3)              // "ababab"
str_reverse("hello")             // "olleh"
str_title("hello world")         // "Hello World"
```

> **Theory — strings in memory**
>
> A string is really just an array of characters plus a length. Under the hood,
> Cnext manages the memory so you don't have to. This is one of the comforts of
> a high-level language — in plain C, handling text is famously fiddly.

> **Real world**
>
> Strings are everywhere: usernames, emails, search queries, JSON messages,
> file paths. Almost any program you write will spend a chunk of its time
> cleaning, joining, searching, and formatting text.

> **Exercise 10.1**
>
> Ask for a user's first and last name, then print their full name in all caps
> using `str_upper`, and its total length.

<details><summary>Suggested solution</summary>

```cnext
main {
    print("First name: ")
    var first = input()
    print("Last name: ")
    var last = input()
    var full = first + " " + last
    printin(str_upper(full))
    printin("Length: " + full.length)
}
```

</details>

---
## 11. Functions

> **Difficulty: BEGINNER**

### 11.1 Definition

- A **function** is a named, reusable block of code. You *define* it once and
  *call* it (use it) as many times as you like. Functions let you avoid
  repeating yourself and give a name to a piece of logic.

### 11.2 A function with parameters and a return value

```cnext
func add(int a, int b): int {
    return a + b
}

main {
    var total = add(5, 3)      // calling the function; total becomes 8
    printin("total: " + total)
    printin("again: " + add(100, 1))
}
```

How to read `func add(int a, int b): int { ... }`:

| Piece | Meaning |
|-------|---------|
| `func` | "I'm defining a function." |
| `add` | The function's name. |
| `(int a, int b)` | The **parameters** (inputs) and their types. |
| `: int` | The **return type** — the type of the value it hands back. |
| `return a + b` | Hands the result back to whoever called it. |

### 11.3 A function with no return value

If a function does something but doesn't give back a value, you just omit the
return type:

```cnext
func greet(str name) {
    printin("Hello, " + name + "!")
}

greet("World")   // Hello, World!
```

### 11.4 Default & named arguments

Give parameters **default** values so callers can omit them:

```cnext
func greet(str name, str greeting = "Hello") {
    printin(greeting + ", " + name + "!")
}

greet("World")                  // Hello, World!
greet("Ada", greeting = "Hi")   // Hi, Ada!
```

When a function has several parameters, you can pass them by **name** so the
order doesn't matter:

```cnext
func create_person(str name, int age, str city = "Unknown") {
    printin(name + " is " + age + " from " + city)
}

create_person(name = "Bob", age = 25, city = "NYC")
create_person(age = 30, name = "Charlie")
```

### 11.5 Variadic functions (any number of arguments)

Use `...` to accept however many arguments you like:

```cnext
func sum(int... args): int {
    var total = 0
    for var n in args {
        total = total + n
    }
    return total
}

printin(sum(1, 2, 3))         // 6
printin(sum(1, 2, 3, 4, 5))   // 15
```

Inside the function, `args` behaves like an array you can loop over.

### 11.6 Lambdas (anonymous functions)

A **lambda** is a short, one-line function you can store in a variable or pass
around. It uses **`=>`** (the arrow):

```cnext
main {
    var double_it = (int x) => x * 2
    printin(double_it(21))   // 42
}
```

> **Warning — when to use `=>`**
>
> The `=>` arrow is for **lambda-style** functions assigned to a variable. A
> `func` declaration always uses `{ ... }`:
>
> ```cnext
> func add(int a, int b): int {   // correct
>     return a + b
> }
> ```
>
> Don't write `func add(int a, int b): int => a + b` — that form is not valid.
> Use `{ ... }`, or assign a lambda to a variable.

> **Theory — why functions matter**
>
> Functions are the fundamental tool for managing **complexity**. Instead of
> one giant wall of code, you split a problem into named, testable pieces.
> This idea — breaking problems into reusable, named units — is called
> **abstraction**, and it's the single most important design skill in software.

> **Real world**
>
> Real programs are almost entirely functions. A banking app has
> `compute_interest(amount, rate)`, `validate_pin(attempted, stored)`, and
> `withdraw(account, amount)`. Each is a small, focused piece of logic that the
> rest of the app calls by name.

> **Exercise 11.1**
>
> Write a function `is_even(int n): bool` that returns `true` if a number is
> even (check `n % 2 == 0`). Use it in a loop to print whether `0`–`4` are even.

<details><summary>Suggested solution</summary>

```cnext
func is_even(int n): bool {
    return n % 2 == 0
}

main {
    for int i = 0; i < 5; i = i + 1 {
        printin(i + " even? " + is_even(i))
    }
}
```

</details>

---
## 12. Handling Errors: `throw` and `try`/`catch`

> **Difficulty: BEGINNER–INTERMEDIATE**

### 12.1 Definition

- **Error handling** is the process of *responding* when something goes wrong,
  instead of letting your program crash. In Cnext you *signal* a problem with
  **`throw`** and *handle* it with **`try`**/`catch` (and optionally
  `finally`).

### 12.2 Throwing and catching

A function can `throw` a message to signal a problem. The caller wraps the
risky call in `try` and grabs the error in `catch`:

```cnext
func divide(int a, int b): int {
    if b == 0 {
        throw "Division by zero"   // signal an error
    }
    return a / b
}

main {
    try {
        divide(10, 0)
    } catch (str err) {
        printin("caught: " + err)  // handle it gracefully
    }
}
```

Output:

```
caught: Division by zero
```

Without the `try`/`catch`, calling `divide(10, 0)` would crash the program.
With it, the program recovers and keeps running.

### 12.3 `finally` — code that always runs

Add a `finally` block for code that must run whether the attempt succeeded or
failed (closing a file, releasing a resource, printing "done"):

```cnext
try {
    var result = divide(10, 2)
    printin("result: " + result)
} catch (str err) {
    printin("error: " + err)
} finally {
    printin("cleanup done")
}
```

> **Key point**
>
> `try`/`catch` separates the *happy path* from the *error path*. The happy
> code reads cleanly in `try`; the error handling lives in `catch`. This is far
> easier to read than scattering error checks everywhere.

> **Theory — errors vs. just returning a bad value**
>
> A function could "return a special value" to signal failure (like `-1`), but
> that is easy to forget and easy to confuse with a real result. **Exceptions**
> (`throw`) let a signal jump straight to the nearest matching `catch`, skipping
> all the intermediate code, so a failed step can't silently continue.

> **Real world**
>
> Errors are reality in software: a file that's missing, a server that's down,
> an empty string where a number was expected. A real app always wraps such
> operations — it should *never* crash on one bad input; it should catch, log,
> and let the user retry.

> **Exercise 12.1**
>
> Write `func safe_sqrt(float x): float` that throws `"negative input"` if `x`
> is negative, otherwise returns `x * x`. Call it from `try`/`catch` with `9`
> and with `-4`, printing a friendly message either way.

<details><summary>Suggested solution</summary>

```cnext
func safe_sqrt(float x): float {
    if x < 0 {
        throw "negative input"
    }
    return x * x
}

main {
    try {
        printin(" => " + safe_sqrt(9.0))
    } catch (str err) {
        printin("caught: " + err)
    }
    try {
        printin(" => " + safe_sqrt(-4.0))
    } catch (str err) {
        printin("caught: " + err)
    }
}
```

</details>

---

## 13. Nullability: Saying "I Don't Know"

<!-- Difficulty: INTERMEDIATE -->

### 13.1 Definition

- **`null`** means "no value." Some types can hold `null` to express "this is
  missing or unknown." **Nullable types** let you mark exactly where this is
  possible, so the compiler can help you avoid crashes.

### 13.2 Nullable types

By default, a type doesn't allow `null`. Add a `?` to make it nullable:

```cnext
str? maybe_name = null      // may or may not hold text
int? maybe_age = 25         // optional number

if maybe_age != null {
    printin(maybe_age)      // safe to use here
}
```

### 13.3 The null-coalescing operator `??`

Give a **default** value when something might be `null`:

```cnext
str name = get_user_name() ?? "Guest"
```

If `get_user_name()` returns `null`, `name` becomes `"Guest"`. Otherwise it
gets the real value.

### 13.4 Optional chaining `?.`

Call methods/fields safely: if the value is `null`, the result is `null`
instead of a crash:

```cnext
str city = user?.address?.city ?? "Unknown"
```

If `user` or `user.address` is `null`, the whole chain safely becomes `null`,
and `??` turns it into `"Unknown"`.

> **Theory — null bugs and how types help**
>
> "Null pointer" crashes are among the most famous bugs in programming.
> In languages where almost any value can silently be null, these bugs only
> surface at runtime. Cnext's `?` types make "this might be null" explicit to
> the compiler, so it can check that you handle the "no value" case — catching
> many bugs before the program even runs.

> **Real world**
> "No value" situations are everywhere: a user has no middle name, a searched
> record wasn't found, a setting isn't configured yet. Representing these
> clearly with nullable types and `??` defaults writes code that stays
> predictable instead of crashing on surprise empty values.

> **Exercise 13.1**
>
> Declare a nullable `int?`, set it to `null`, then print either its value or a
> fallback using `?? ` when a default for one case.

<details><summary>Suggested solution</summary>

```cnext
main {
    int? c = null
    printin(c ?? 0)      // prints 0 because c is null
    c = 7
    printin(c ?? 0)      // prints 7
}
```

</details>

---
# PART III — Objects & Data

> The tools for modeling the real world: structs, enums, classes, and generics.

---

## 14. Structs, Enums & Pattern Matching

> **Difficulty: BEGINNER–INTERMEDIATE**

### 14.1 Definition

- A **struct** groups a few related values together under one name.
- An **enum** (enumeration) names a fixed set of choices.
- **Pattern matching** (`match`) picks which code to run based on which choice
  a value is.

### 14.2 Structs — a bundle of data

```cnext
struct Point {
    int x
    int y
}

Point p = Point { x: 10, y: 20 }
printin(p.x)  // 10
printin(p.y)  // 20

p.y = 35               // you can change a field
printin(p.y)           // 35
```

You can also set fields after creation:

```cnext
struct User {
    str name
    int age
}

User u
u.name = "Ada"
u.age = 36
```

> **Key point**
>
> Use a **struct** for a small group of plain data values. Use a **class**
> (next chapter) when that data also needs *behavior* — methods, inheritance,
> and more.

### 14.3 Enums — a fixed set of choices

An **enum** lists the possible values something can hold:

```cnext
enum Color {
    RED,
    GREEN,
    BLUE
}

enum HttpStatus {
    OK = 200,
    NOT_FOUND = 404,
    ERROR = 500
}

Color c = GREEN
printin(c == GREEN)   // true
```

### 14.4 Pattern matching with `match`

`match` checks a value against each case and runs the matching branch:

```cnext
enum Shape {
    CIRCLE,
    RECTANGLE,
    TRIANGLE
}

func describe(Shape s): str {
    match s {
        CIRCLE    => "Circle"
        RECTANGLE => "Rectangle"
        TRIANGLE  => "Triangle"
        _         => "Unknown"
    }
}
```

The `_` wildcard catches anything not matched — like `else`.

> **Theory — data modeling**
>
> Real systems are modeled with these building blocks: a struct is a *record*
> (this order has a number, a total, a status), and an enum is a *closed set of
> choices* (order status: `NEW`, `PAID`, `SHIPPED`, `CANCELLED`). Combined with
> `match`, they let you encode the *possibilities* of your domain directly in
> the type system, so invalid states become hard or impossible to express.

> **Real world**
>
> Every user-facing choice set is an enum in disguise: notification types,
> payment methods, product categories. And structs are everywhere — a record in
> a database, a row in a spreadsheet, one message in a chat.

> **Exercise 14.1**
>
> Define an `enum Direction { NORTH, SOUTH, EAST, WEST }` and a function
> `func opposite(Direction d): str` that returns the opposite direction's name
> (N↔S, E↔W) using `match`.

<details><summary>Suggested solution</summary>

```cnext
enum Direction {
    NORTH,
    SOUTH,
    EAST,
    WEST
}

func opposite(Direction d): str {
    match d {
        NORTH => "South"
        SOUTH => "North"
        EAST  => "West"
        WEST  => "East"
    }
}

main {
    printin(opposite(NORTH))  // South
}
```

</details>

---
## 15. Classes & Object-Oriented Programming

> **Difficulty: INTERMEDIATE**

### 15.1 Definition

- A **class** is a blueprint for creating **objects**. An object bundles
  **data** (fields) *and* **behavior** (methods) together. This style of
  programming is called **object-oriented programming (OOP)**.

### 15.2 A simple class

```cnext
class Counter {
    int count

    func new(int start) {     // this builds (constructs) a new Counter
        self.count = start    // `self` refers to the object we're building
    }

    func bump() {
        self.count = self.count + 1
    }

    func value(): int {
        return self.count
    }
}

main {
    Counter c = new Counter(10)  // make a Counter starting at 10
    c.bump()
    c.bump()
    printin(c.value())           // 12
}
```

Key terms:
- **Class** — the blueprint (`class Counter { ... }`).
- **Object** — one instance created from the blueprint (`new Counter(10)`).
- **Constructor** — the special `new` method that sets up a new object.
- **`self`** — inside a method, `self` refers to the object the method was
  called on.

### 15.3 A class with behavior

```cnext
class Circle {
    float radius

    func new(float r) {
        self.radius = r
    }

    func area(): float {
        return 3.14159 * self.radius * self.radius
    }
}

Circle c = new Circle(5.0)
printin(c.area())          // ~78.53975
```

### 15.4 Inheritance

A class can **extend** another, inheriting its fields and methods, and
**override** behavior:

```cnext
class Animal {
    str name

    func new(str n) {
        self.name = n
    }

    func speak() {
        printin("...")
    }
}

class Dog extends Animal {
    func new(str n) {
        super.new(n)          // call the parent constructor
    }

    override func speak() {
        printin("Woof!")
    }
}

Dog d = new Dog("Rex")
d.speak()                     // Woof!
```
### 15.5 Interfaces & traits

An **interface** declares *what* methods a class must have. A **trait** can
also provide *default* behavior. Both let different classes be used in the
same way:

```cnext
interface Drawable {
    func draw()
}

class Square implements Drawable {
    override func draw() {
        printin("Drawing square")
    }
}
```

> **Theory — abstraction, encapsulation, inheritance**
>
> OOP rests on three big ideas:
> - **Encapsulation** — keep an object's data and its rules inside the object;
>   outsiders use its methods, not its internals.
> - **Abstraction** — an interface promises *what* an object can do without
>   saying *how*.
> - **Inheritance** — reuse common behavior by building new classes on top of
>   existing ones.
> Together they let large programs be built from small, predictable pieces.

> **Real world**
>
> Almost any GUI, game, or web framework uses classes: a `Button` and a
> `TextBox` both *are* `Drawable`; a game's `Player`, `Enemy`, and `Item`
> inherit from a common entity with shared position and movement. OOP mirrors
> how we naturally categorize real-world things.

> **Exercise 15.1**
>
> Create a `class Rectangle` with `width` and `height`, a constructor, and an
> `area(): float` method. Build two rectangles and print both areas.

<details><summary>Suggested solution</summary>

```cnext
class Rectangle {
    float width
    float height

    func new(float w, float h) {
        self.width = w
        self.height = h
    }

    func area(): float {
        return self.width * self.height
    }
}

main {
    var a = new Rectangle(3.0, 4.0)
    var b = new Rectangle(5.0, 6.0)
    printin(a.area())   // 12.0
    printin(b.area())   // 30.0
}
```

</details>

---

## 16. Generics: Code That Works with Any Type

> **Difficulty: INTERMEDIATE**

### 16.1 Definition

- **Generics** let you write a class or function once and use it with *many
  different types*, instead of copying it for each type.

### 16.2 A generic class

A generic class in Cnext uses angle brackets `<T>` for a type placeholder:

```cnext
class Box<T> {
    T value

    func new(T v) {
        self.value = v
    }

    func get(): T {
        return self.value
    }
}

var int_box = new Box<int>(42)
var str_box = new Box<str>("hello")

printin(int_box.get())   // 42
printin(str_box.get())   // hello
```

Here `T` is a **type parameter** — a stand-in that becomes `int` for
`Box<int>` and `str` for `Box<str>` when you create them.

### 16.3 Why generics matter

Without generics, you'd write a separate `IntBox`, `StrBox`, `FloatBox`, and so
on — one for each type. Generics let you write the logic **once**.

> **Theory — "code once, use for many types"**
>
> Generics are a form of **polymorphism** (Greek for "many shapes") — the same
> code *shape* adapts to different concrete types. This removes duplication,
> and because Cnext is statically typed, the right concrete type is checked at
> compile time.

> **Real world**
>
> Collections are the classic generic: a `List<string>`, a
> `Queue<Message>`, a `Map<AccountId, Balance>`. Frameworks use generics so one
> reusable library serves every project without forcing everything into a
> single "object" type.

> **Exercise 16.1**
>
> Use the `Box<T>` class above to store both a `bool` and a `double`, and print
> each value.

<details><summary>Suggested solution</summary>

```cnext
var flag_box = new Box<bool>(true)
var price_box = new Box<double>(9.99)
printin(flag_box.get())    // true
printin(price_box.get())   // 9.99
```

</details>

---
# PART IV — Modern Features

> The features that make Cnext powerful: closures, generators, async, and
> threading.

---

## 17. Closures & Higher-Order Functions

> **Difficulty: INTERMEDIATE**

### 17.1 Definition

- A **higher-order function** is a function that takes *another function* as an
  argument (or returns one).
- A **closure** is a function that *captures* variables from the place where it
  was created, even after that place has finished.

### 17.2 Passing a function to a function

Because lambdas are values, you can hand one to another function:

```cnext
func apply(int x, func f): int {
    return f(x)
}

var double_it = (int n) => n * 2
var triple_it = (int n) => n * 3

printin(apply(5, double_it))   // 10
printin(apply(5, triple_it))   // 15
```

`apply` doesn't care *which* function it gets — it just calls `f(x)`.

### 17.3 A closure captures its surroundings

A lambda can "remember" variables from the scope around it:

```cnext
var x = 10
var add_x = (int a) => a + x   // captures x
printin(add_x(5))              // 15   (5 + 10)
```

### 17.4 Reusable policies with closures

Closures are great for *customizing* shared logic. Here a single `apply`
function is given different rules:

```cnext
func apply_to_price(float price, func discount): float {
    return discount(price)
}

var student = (float p) => p * 0.9      // 10% off
var member  = (float p) => p * 0.8      // 20% off

printin(apply_to_price(100.0, student)) // 90.0
printin(apply_to_price(100.0, member))  // 80.0
```

> **Theory — functions as values**
>
> Treating functions as data is a cornerstone of **functional programming**.
> It's powerful because you can *parametrize* behavior: the same code runs with
> different rules swapped in. Under the hood, a closure bundles the function
> with the captured variables' values, so the combination can be passed around
> as one unit.

> **Real world**
>
> Gui buttons take a "what to do when clicked" callback. Sorting takes a
> "which is smaller" comparator. Web routes take a "handle this request"
> handler. Everywhere you see a function handed to another function as a
> setting, you're seeing closures and higher-order functions in action.

> **Exercise 17.1**
>
> Write `func repeat(int times, func f)` that calls `f` that many times. Call it
> with a lambda that prints `"beep"`. (A `func f` without a return type is
> called as a plain statement.)

<details><summary>Suggested solution</summary>

```cnext
func repeat(int times, func f) {
    for int i = 0; i < times; i = i + 1 {
        f()
    }
}

main {
    repeat(3, () => printin("beep"))  // beep x3
}
```

</details>

---

## 18. Generators & Coroutines

> **Difficulty: INTERMEDIATE–ADVANCED**

### 18.1 Definition

- A **generator** is a function that *yields* a sequence of values one at a
  time, pausing between each. You consume it with a `for` loop.
- A **coroutine** is a resumable function — it can pause (`yield`) and later be
  resumed right where it left off.

### 18.2 A generator with `yield`

```cnext
func count_to(int n): iter<int> {
    for int i = 1; i <= n; i = i + 1 {
        yield i
    }
}

for var val in count_to(5) {
    printin(val)     // 1, 2, 3, 4, 5
}
```

`yield` hands a value back *and remembers* the current position, so the next
turn of the loop continues one step further.

### 18.3 An infinite generator with a break

Fibonacci is a famous generator — there's no upper bound, and the *consumer*
decides when to stop:

```cnext
func fibonacci(): iter<int> {
    var a = 0, b = 1
    while true {
        yield a
        var temp = a + b
        a = b
        b = temp
    }
}

for var x in fibonacci() {
    if x > 100 { break }
    printin(x)   // 0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89
}
```

### 18.4 Why "lazy" is valuable

A generator computes each value **only when asked**. It doesn't build the whole
sequence in memory:

> **Key point**
>
> Generators are **lazy**: values are produced on demand. That's why you can
> make a Fibonacci sequence that is effectively infinite — nothing is stored,
> and nothing runs until the loop asks for the next value.

> **Theory — the "what runs when" behind lazy iteration**
>
> A generator is compiled into a small state machine. Each `yield` saves the
> position and local variables; each resume reloads them and continues just
> after the `yield`. This is how one function body can "own" a loop across many
> separate calls from the `for` loop.

> **Real world**
>
> Generators stream data: infinite number sequences, reading a huge file line
> by line (so it never sits in memory at once), and paging through API results
> one page at a time. Any "I have a stream of values" problem is a natural fit.

> **Exercise 18.1**
>
> Write a generator `func evens_up_to(int n): iter<int>` that yields even
> numbers from `0` up to `n`, then print them for `n = 10`.

<details><summary>Suggested solution</summary>

```cnext
func evens_up_to(int n): iter<int> {
    for int i = 0; i <= n; i = i + 1 {
        if i % 2 == 0 {
            yield i
        }
    }
}

main {
    for var e in evens_up_to(10) {
        printin(e)   // 0 2 4 6 8 10
    }
}
```

</details>

---
## 19. Async: Doing I/O Without Blocking

> **Difficulty: ADVANCED**

### 19.1 Definition

- **Asynchronous programming** lets a program *start* a slow operation (like a
  network request or a file read) and **keep doing other work** while it
  finishes — instead of freezing until it's done.

### 19.2 `async func` and `await`

Mark a function `async` if it may need to wait. Inside it, `await` pauses the
function until a result is ready:

```cnext
async func fetch_page(str url): str {
    // Some operation that takes time (here simplified)
    return "content from " + url
}

async func main_async() {
    var content = await fetch_page("https://example.com")
    printin(content)
}
```

The key point is that while `fetch_page` is waiting, the rest of your program
is **not** frozen — the runtime can service other work.

### 19.3 A mental model

Think of async as "cooperative scheduling": each `async` function is a task
that can choose to *pause and let others run* at every `await`.

> **Key point**
>
> `async` marks a function that can wait; `await` is where it actually pauses.
> Between `await` points, the function runs normally and synchronously.

> **Theory — why async beats blocking for I/O**
>
> Blocking I/O wastes time: a CPU sitting idle waiting for a network response
> does nothing useful. Async keeps the CPU busy by running other tasks in the
> gaps. This is how one thread can handle thousands of in-flight requests —
> not by doing them all at once, but by never *idle-waiting*.

> **Real world**
>
> Web servers, chat apps, and mobile apps all use async. When your photo
> uploads "in the background" while the UI stays responsive, or a web server
> serves thousands of users at once, async `await` is why.

> **Exercise 19.1**
>
> Try the pattern: write a small `async func parse_config(str text): str` that
> `return`s `text` trimmed, and `await` it inside an `async main_async` that
> prints the result.

<details><summary>Suggested solution</summary>

```cnext
async func parse_config(str text): str {
    return str_trim(text)
}

async func main_async() {
    var clean = await parse_config("  api_key=abc  ")
    printin(clean)   // "api_key=abc"
}
```

</details>

---

## 20. Threading: Doing Two Things at Once

> **Difficulty: ADVANCED**

### 20.1 Definition

- **Threading** runs multiple pieces of code **at the same time** on different
  CPU cores, using multiple **threads** — separate execution paths inside one
  program.

### 20.2 Why threads are risky: shared data

Async (last chapter) interleaves work on *one* thread. Threading runs work on
*many* threads truly simultaneously. That speed comes with a catch: if two
threads change the *same* variable at once, they can corrupt each other.

> **Key point**
>
> When several threads read/write shared data, you need **synchronization** —
> rules that only one thread touches a piece of data at a time. Cnext provides
> `Mutex` (a mutual-exclusion lock) and `Channel` (a safe way to pass messages
> between threads).

### 20.3 The two safe patterns

1. **Channels** — threads *send* and *receive* messages, never sharing
   variables.
2. **Mutex** — guard a shared variable so only one thread enters a critical
   section at a time.

```cnext
// Conceptual sketch — a Mutex guards a shared counter.
Mutex m = new Mutex()
int shared = 0

// inside a worker thread:
m.lock()
shared = shared + 1
m.unlock()
```

> **Theory — race conditions & deadlocks**
>
> Two threads updating the same value without a lock can interleave badly —
> a **race condition** that produces wrong results only sometimes. A `Mutex`
> prevents that by making the update atomic. The matching danger is a
> **deadlock**: thread A waits for B while B waits for A, and both freeze.
> The safe guiding design principles are: share via messages (channels) when
> possible, and keep each locked region tiny.

> **Real world**
>
> Threading speeds up CPU-heavy work: image/video processing, physics in games,
> and parallel mathematics. Async handles I/O-bound waiting; threading handles
> compute-bound speedups. Real systems combine both.

> **Exercise 20.1 (conceptual)**
>
> In your own words, explain the difference between async/await and threads, and
> give one task each is good for. (There's no code to run for this one.)

---
# PART V — The Professional Toolkit

> The tools and library that turn "I can write programs" into "I can build
> real software."

---

## 21. The Tools: run, build, fmt, lint, repl, test, doctor

> **Difficulty: BEGINNER–INTERMEDIATE**

### 21.1 Definition

- The **`cnext` command line tool** bundles a compiler plus a small toolbox:
  run, build, format, lint, REPL, test, and a "doctor" that checks your setup.

### 21.2 The everyday commands

| Command | What it does |
|---------|--------------|
| `cnext run file.cn` | Compile and immediately run your program. |
| `cnext build file.cn` | Compile into a standalone executable (default `out`/`out.exe`). |
| `cnext build file.cn --release` | Optimized release build with no debug info. |
| `cnext build file.cn --debug` | Debug build with symbols, no optimization. |
| `cnext repl` | Interactive prompt for experiments. |
| `cnext fmt file.cn` | Automatically lay out your code neatly. |
| `cnext lint file.cn` | Check for common mistakes and suspicious patterns. |
| `cnext test` | Run your program's test suite. |
| `cnext new project_name` | Scaffold a fresh project folder. |
| `cnext doctor` | Check that your system is ready for Cnext. |
| `cnext help` / `cnext version` | Show help / version info. |

### 21.3 Why formatting and linting matter

- **`cnext fmt`** makes everyone's code look the same, which makes teams (and
  your future self) able to diff and review changes easily.
- **`cnext lint`** catches real bug patterns — unused variables, unreachable
  code, suspicious comparisons — *before* you even run the program.

> **Key point**
>
> A professional workflow is: **run** often while developing, **build** when
> you want an artifact, **fmt** + **lint** before committing, and **test** to
> prove your code works.

> **Theory — compilers vs. toolchains**
>
> A **compiler** only translates code. A **toolchain** adds the surrounding
> quality-of-life tools — formatters, linters, testers — that turn "it
> compiles" into "it is correct and maintainable." Mature languages all ship
> these; Cnext bundles them in one command.

> **Real world**
>
> In any real project, "it builds" is the bare minimum. Continuous integration
> (CI) runs `fmt`, `lint`, and `test` on every change — automated quality
> gates. Learning these commands *now* means you already understand what CI
> checks.

> **Exercise 21.1**
>
> Take any `.cn` file you've written in this book and run these in order:
> `cnext fmt`, `cnext lint`, `cnext build`, then run the built executable. Note
> what each command reports.

---
## 22. The Standard Library & Global Built-ins

> **Difficulty: INTERMEDIATE**

### 22.1 Definition

- The **standard library** is the set of ready-made functions and types that
  ship with the language — you don't have to write them yourself.

### 22.2 Built-in categories

Cnext provides **170+ global built-in functions** you can call directly (no
imports needed). They fall into a few familiar families:

**Numbers & math**
```cnext
math_abs(-5)                  // 5
math_min(5, 3)                // 3
math_max(5, 3)                // 5
math_clamp(10, 0, 5)          // 5
math_gcd(12, 8)               // 4
math_factorial(5)             // 120
math_fibonacci(10)            // 55
```

**Type conversion**
```cnext
to_int(3.14)                  // 3
to_float(42)                  // 42.0
to_str(42)                    // "42"
to_bool(1)                    // true
ord("A")                      // 65
```

**String utilities**
```cnext
str_upper("hi")               // "HI"
str_trim("  hi  ")            // "hi"
str_contains("hello", "ell")  // true
str_replace("hello", "l", "r")// "herro"
str_substring("hello", 1, 3)  // "el"
str_split("a,b,c", ",")       // ["a", "b", "c"]
str_join(["a","b"], ", ")     // "a, b"
```

**File & system**
```cnext
write_file("notes.txt", "hello")
str content = read_file("notes.txt")
bool exists = file_exists("notes.txt")
long size = file_size("notes.txt")

str os   = platform()     // e.g. "windows"
int cpus = cpu_count()
str home = home_dir()
```

### 22.3 Files, JSON, networking

The standard library also covers bigger areas:

- **Files** — read/write/check files and paths.
- **JSON** — parse and build JSON data (the format APIs use).
- **Networking** — make HTTP requests and even run a small HTTP server.
- **Encoding & crypto** — base64, hashing, and friends.

> **Key point**
>
> You almost never need to re-invent common tasks — the standard library has
> them. The skill is *knowing what exists* and looking it up, not memorizing
> every name.

> **Theory — why a good stdlib matters**
>
> "Not invented here" is a classic failure: teams re-building basics badly.
> A rich standard library lets programmers compose *well-tested, consistent*
> building blocks, which is faster and safer than hand-rolling. It's the
> difference between building a house from beams vs. from raw logs.

> **Real world**
>
> Reading a config file, saving user data, talking to a weather API, hashing a
> password — every real app leans on these libraries constantly.

> **Exercise 22.1**
>
> Write a program that writes `"hello from cnext"` to `test.txt`, reads it
> back, uppercases it with `str_upper`, and prints the result.

<details><summary>Suggested solution</summary>

```cnext
main {
    write_file("test.txt", "hello from cnext")
    var text = read_file("test.txt")
    printin(str_upper(text))   // HELLO FROM CNEXT
}
```

</details>

---
## 23. Projects, Packages & Real Software

> **Difficulty: INTERMEDIATE**

### 23.1 Definition

- A **project** is a folder that contains your program's source code, its
  configuration, and any **packages** it depends on. Packages are reusable
  libraries you pull in from a registry instead of writing yourself.

### 23.2 Creating a project

```bash
cnext new my_app
cd my_app
```

This scaffolds a clean, standard layout:

```
my_app/
├── src/
│   ├── main.cn          # entry point
│   ├── greeting.cn      # your functions
│   └── models.cn        # your data classes
├── cnext.toml           # project configuration
└── README.md
```

> **Key point**
>
> Organizing code into files by *responsibility* (`main`,
> `greeting`/utilities, `models`/data) is how real codebases stay readable as
> they grow. One giant file does not scale.

### 23.3 The `cnext.toml` config file

`cnext.toml` stores your project's name, version, and dependencies. It's the
single source of truth that `cnext` reads to build, test, and install
packages.

### 23.4 Packages

Cnext includes a built-in **package manager**:

| Command | What it does |
|---------|--------------|
| `cnext install` | Install all dependencies listed in `cnext.toml`. |
| `cnext install <pkg>` | Install a single package. |
| `cnext remove <pkg>` | Remove a package. |
| `cnext update` | Update all dependencies. |
| `cnext search <q>` | Search the registry. |
| `cnext info <pkg>` | Show package details. |

> **Theory — dependencies & why they're double-edged**
>
> Pulling in well-maintained packages saves huge amounts of time — you reuse
> solutions instead of re-inventing them. The catch: every dependency is code
> you *trust*, so real projects version-pin dependencies and review what they
> add. This is why `cnext.toml` records versions precisely.

> **Real world**
>
> Every production project has dependencies: HTTP clients, JSON parsers,
> logging, tests. Managing them cleanly with a package manager and a single
> `cnext.toml` is standard practice across all languages.

> **Exercise 23.1**
>
> Run `cnext new first_project`, open the generated `cnext.toml` and
> `src/main.cn`, and confirm `cnext run src/main.cn` works.

---
## 24. Capstone: Build a Real Mini-Project

> **Difficulty: INTERMEDIATE** · *Put it all together.*

### 24.1 The goal

Let's combine many chapters into one genuinely useful program: a **grade
book** that

1. stores several students (struct/class) with scores,
2. reads input with `input`,
3. decides pass/fail with `if` (Chapter 7),
4. loops over the list (Chapter 8),
5. uses functions (Chapter 11),
6. and handles bad input with `try`/`catch` (Chapter 12).

### 24.2 The program

```cnext
class Student {
    str name
    int score

    func new(str n, int s) {
        self.name = n
        self.score = s
    }

    func passed(): bool {
        return self.score >= 60
    }
}

func report(Student s) {
    var result = s.passed() ? "PASS" : "FAIL"
    printin("{s.name}: {s.score} -> {result}")
}

main {
    Student[] room = {
        new Student("Ada", 88),
        new Student("Bob", 45),
        new Student("Cy", 92)
    }

    for var s in room {
        report(s)
    }
}
```

Run it:

```bash
cnext run grades.cn
```

Output:

```
Ada: 88 -> PASS
Bob: 45 -> FAIL
Cy: 92 -> PASS
```

### 24.3 Make it interactive

Extend it to read scores from the user and handle bad input gracefully:

```cnext
main {
    print("Enter a score (0-100): ")
    var raw = input()
    try {
        var n = to_int(raw)
        printin(n >= 60 ? "PASS" : "FAIL")
    } catch (str err) {
        printin("That wasn't a number: " + err)
    }
}
```

> **Exercise 24.1**
>
> Extend the grade book: read several student names+scores from the user,
> store them in the `Student[]` array, then print the class **average** score.
> Use a loop to sum the scores and divide by the count.

<details><summary>Suggested solution</summary>

```cnext
class Student {
    str name
    int score

    func new(str n, int s) {
        self.name = n
        self.score = s
    }
}

main {
    var count = 0
    var total = 0
    var more = "yes"

    while more == "yes" {
        print("Student name: ")
        var name = input()
        print("Score: ")
        var score = to_int(input())
        total = total + score
        count = count + 1
        print("Another? (yes/no): ")
        more = input()
    }

    if count > 0 {
        printin("Average: " + (total / count))
    }
}
```

</details>

> **Congratulations!** You have gone from "what is a variable?" to building
> programs with classes, control flow, functions, arrays, error handling, and
> real project structure. That is a genuine, valuable foundation.

---
# Reference & Appendices

---

## Appendix A — Glossary

**Compiler** — a tool that turns source code into an executable program. Cnext
turns `.cn` into C, then GCC/Clang finish the job.

**Variable** – a named container that stores a value.

**Type** – describes what kind of value a variable holds (number, text, etc.).

**`var`** – lets Cnext infer (guess) a variable's type from its value.

**Constant (`const`)** – a value that cannot be changed after being set.

**String (`str`)** – text, written with double quotes.

**Character (`char`)** – one character, written with single quotes.

**Boolean (`bool`)** – a value that is either `true` or `false`.

**Array** – an ordered list of values of the same type; indexes start at 0.

**Loop** – a way to repeat a block of code (`while`, `for`, `for-in`).

**Condition** – a true/false check used by `if` and `while`.

**Operator** – a symbol that performs a calculation (`+`, `-`, `==`, etc.).

**Function** – a named, reusable block of code.

**Lambda** – a short anonymous function written with `=>`.

**Class** – a blueprint for objects that bundle data and behavior together.

**Object** – one instance created from a class.

**`self`** – inside a method, refers to the object the method was called on.

**Struct** – a small group of plain data values bundled under one name.

**Enum** – a named set of fixed choices.

**`match`** – an expression that runs the case matching a value; `_` is the
"anything else" wildcard.

**`main`** – the special function where every program starts running.

**`throw` / `try` / `catch`** – signaling a problem and handling it gracefully.

**`null`** – "no value"; nullable types (`str?`) allow it.

**`??`** – null-coalescing: use a default value when something is `null`.

**Generics** – code written once that works with many types (`Box<T>`).

**Closure** – a lambda that captures variables from where it was created.

**Generator** – a function that `yield`s a sequence lazily.

**Async/await** – doing slow I/O without blocking the rest of the program.

**Threading** – running code simultaneously on multiple CPU cores.

**REPL** – an interactive prompt that reads, evaluates, and prints.

**`main`** — where every program begins.

---

## Appendix B — Troubleshooting

| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| `cnext: command not found` | Executable not on PATH | Re-check the install step (Chapter 2). |
| `gcc not found` / errors mention `gcc` | No C compiler | Install GCC or Clang and add it to PATH. |
| Error with a line number and `^` | A syntax/type error there | Read that exact line carefully. |
| Program never stops | Infinite loop | Make sure the counter is updated. |
| Unexpected `out of bounds` crash | Accessing `arr[idx]` beyond length | Check indexes; remember they start at 0. |
| `var` inferred the wrong type | Value implied a different type | Use an explicit type annotation. |
| Still stuck | – | See the repos's issues / community; include your `cnext version`. |

---

## Appendix C — Quick Keyword & Operator Reference

**Declaring:** `var x = ...`, `int y = 5`, `const int N = 3`,
`type Alias = ...`.

**Types:** `int`, `long`, `float`, `double`, `str`, `bool`, `char`, `byte`,
`uint`, `ulong`, `ushort`, `ubyte`, plus nullable variants with `?`.

**Operators:** `+ - * / %`, `== != < > <= >=`, `&& || !`, `+= -= *= /=`,
ternary `? :`, `as` conversion, `??` empty-fallback, `?.` optional access.

**Control:** `if / else if / else`, `match`, `while`, `for`, `for-in`,
`break`, `continue`.

**Functions:** `func name(params): T { }`, default args, named args,
variadic `...`, lambdas `(int x) => ...`.

**Data & OOP:** `struct`, `enum`, `class`, `new`, `self`, `extends`,
`override`, `implements`, `interface`, `trait`, generics `Class<T>`.

**Errors/Null:** `throw`, `try/catch/finally`, nullable `?`, `??`, `?.`.

**Advanced:** `yield` (generators), `coroutine`, `async func`/`await`,
closures, extension methods, operator overloading.

---

## Appendix D — Your Next Steps

You have finished the fundamentals. Here is a path to keep growing:

1. **Practice** – redo every exercise and extend the capstone (add a
   delete-a-student feature, save results to a file with `write_file`).
2. **Read the official docs** – types, functions, classes, and the standard
   library reference in the `docs/` folder of the repo.
3. **Study the `examples/` programs** — calculator, to-do CLI, HTTP server,
   threading, and more are all in the repo.
4. **Build your own tool** — a to-do list that saves to disk, a small
   temperature-to-Celsius converter, or a text-based number game.
5. **Learn testing** – write small programs that verify each function.
6. **Read other code** – open-source Cnext projects teach real-world layout.
7. **Explore advanced topics** – generics, closures, async, threading (see
   the ADVANCED chapters and the repo's `docs/`).

> **Final thought**
>
> You now understand what a compiler is, how variables, functions, loops, and
> classes work, and how to build and run real programs. Read a little, type a
> lot, and break things — that is how a language becomes yours.

---

*This guide is an introductory learning resource. It's based on the publicly
documented behavior of Cnext and is meant to complement the authoritative
repository documentation, which should always be consulted for the exact,
current details of any feature.*