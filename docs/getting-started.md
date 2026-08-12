# Getting Started with Cnext — a beginner-friendly guide

Welcome! Cnext is a **programming language**. This guide assumes you have **never
programmed before** (or are new to Cnext) and walks you through everything step
by step. By the end you will have written and run your own programs.

---

## 1. What is Cnext?

A **programming language** is a set of rules for telling a computer what to do.
You write those instructions in a plain text file with a `.cn` extension, and a
special tool called a **compiler** turns them into an executable program that
your computer can run.

```
your code (hello.cn)  ---compiler--->  executable (cnext)  ---run--->  output
```

Cnext is a **high-level** language: it reads like English and handles a lot of
low-level details for you. Under the hood it compiles down to **C**, which is
fast and runs almost anywhere. You don't need to know C to use Cnext — but the
compiler needs **GCC** or **Clang** installed to finish the job (step 2).

In this guide you will often see `printin("...")`. That prints text to the
screen and is the easiest way to see what your program is doing.

---

## 2. What you'll need

- **A text editor** (VS Code, Notepad++, or any editor). You write code in it.
- **GCC or Clang** (a C compiler). On Linux this is usually `gcc`; on
  **Windows** install **MinGW (MSYS2/UCRT64)** or use a toolchain that provides
  `gcc`; on **macOS** install Xcode Command Line Tools.
- **Git** (optional, to clone the repository).
- **Make** (used by the build process).

### Install Cnext from source

```bash
git clone https://github.com/Melton122/cnext.git
cd cnext
make
```

This builds the `cnext` executable in the project root. You can also download a
pre-built package for Windows (`cnext-windows-x64-10.0.0.zip`).

### Put `cnext` on your PATH

So you can type `cnext` from any folder:

- **Linux/macOS:**
  ```bash
  export PATH=$PATH:/path/to/cnext
  ```
- **Windows:** add the folder containing `cnext.exe` to your system PATH, or use
  the included `install.bat` / `install.ps1`.

### Check that it works

```bash
cnext help
```

You should see a list of commands (run, build, fmt, repl, and more). If you see
`command not found` or `not recognized`, the `cnext` executable is not on your
PATH yet — see Troubleshooting at the end.

---

## 3. Your very first program

Create a new file called `hello.cn` and type exactly this:

```cnext
main {
    printin("Hello, World!")
}
```

That's a whole program! Now run it:

```bash
cnext run hello.cn
```

Output:

```
Hello, World!
```

Congratulations — you wrote your first Cnext program! 🎉 Let's look at what just
happened, line by line:

| Line | What it means |
|------|---------------|
| `main {` | Every program needs one `main`. The computer starts executing your code *here*. The `{` opens the body of `main`. |
| `    printin("Hello, World!")` | Call the **built-in function** `printin` to print text. `printin` prints a line (the **in** means *it ends with a newline*, so the next output starts on a fresh line). The text to print goes inside double quotes `"..."`. |
| `}` | Closes the body of `main`. |

> **Key idea:** `main { ... }` is where your program begins. Anything you want
> to happen goes between the `{` and the `}`.

---

## 4. How running and building work

There are two commands that matter for now:

- `cnext run hello.cn` — **compile and run** in one step. Great for playing
  around.
- `cnext build hello.cn` — **compile only**, creating an executable file
  (default name `out` on Linux/macOS, `out.exe` on Windows). You then run that
  file yourself.

Also useful:

- `cnext fmt hello.cn` — automatically formats (lays out) your code neatly.
- `cnext lint hello.cn` — checks your code for common mistakes.

---

## 5. Printing and comments

**`print`** prints text *without* a newline at the end. **`printin`** prints a
*line* (adds a newline). Compare:

```cnext
main {
    print("Hello")
    print("World")
    printin("!")
    printin("second line")
}
```

Output:

```
HelloWorld!
second line
```

Notice `Hello` and `World` print right next to each other because `print` has no
newline, and `!` ends that line because `printin` adds one.

**Comments** are notes to yourself (or others) that the computer ignores. They
start with `//`:

```cnext
// This is a comment. It does nothing.
printin("Hi")  // You can put a comment after code too
```
---

## 6. Variables and types

A **variable** is a named box that holds a value. You create one with
`type name = value`, or use `var` to let Cnext figure out the type for you.

```cnext
main {
    int age = 25            // whole number
    float price = 9.99      // number with a decimal point
    str name = "Cnext"      // text (a string)
    bool is_fun = true      // true or false
    char letter = 'Q'       // a single character (single quotes!)

    printin("Age: " + age)
    printin("Price: " + price)
    printin("Name: " + name)
    printin("Fun? " + is_fun)

    var anything = 100      // var: Cnext infers it's an int
    var also_a_string = "Hi"  // var: inferred as str
}
```

Output:

```
Age: 25
Price: 9.990000
Name: Cnext
Fun? true
```

A few things to notice:

- **Strings** use double quotes `"..."`. **Characters** use single quotes `'...'`.
- `+` joins text and values together (this is called **concatenation**).
- Decimal numbers print with trailing zeroes (`9.990000`). That's just how C
  formats them — don't worry about it for now.
- Use `var` when you want the type guessed for you. Use an explicit type like
  `int`, `float`, `str`, or `bool` when you want to be clear.

### The basic types at a glance

| Type | Meaning | Example |
|------|---------|---------|
| `int` | Whole number | `int n = 42` |
| `float` | Decimal number | `float f = 3.14` |
| `str` | Text | `str s = "Hi"` |
| `bool` | True or false | `bool b = true` |
| `char` | One character | `char c = 'x'` |

---

## 7. Strings and interpolation

A **string** is just text. You can put a variable's value *inside* a string
using curly braces `{ }` — this is called **interpolation**:

```cnext
main {
    str name = "Ada"
    printin("Hello, {name}!")   // inserts the value of name
    printin("2 + 3 = {2 + 3}")  // you can even put a small expression
}
```

Output:

```
Hello, Ada!
2 + 3 = 5
```

> **Note:** interpolation uses `{name}`, not `$name`. `$name` prints the dollar
> sign literally.

---

## 8. Making decisions with `if`

An `if` statement runs code only when a condition is true. `else` runs when it
is false.

```cnext
main {
    int score = 85

    if score >= 50 {
        printin("You passed!")
    } else {
        printin("Try again.")
    }
}
```

Output:

```
You passed!
```

Comparison operators: `==` (equal), `!=` (not equal), `<`, `>`, `<=`, `>=`.

---

## 9. Repeating with loops

### `while` — repeat as long as a condition is true

```cnext
main {
    var i = 0
    while i < 3 {
        printin("i = " + i)
        i = i + 1        // don't forget to increase i, or this loops forever!
    }
}
```

Output:

```
i = 0
i = 1
i = 2
```

### `for` — repeat a known number of times

```cnext
main {
    for int j = 0; j < 3; j = j + 1 {
        printin("j = " + j)
    }
}
```

The `for` has three parts: start value (`int j = 0`), keep-going condition
(`j < 3`), and update (`j = j + 1`).

---

## 10. Arrays

An **array** is an ordered list of values. It is written with `{ }`:

```cnext
main {
    int[] nums = {1, 2, 3, 4, 5}

    printin("length: " + nums.length)  // how many items
    printin("first:  " + nums[0])      // first item (index 0)
    printin("third:  " + nums[2])      // third item (index 2)
}
```

Output:

```
length: 5
first:  1
third:  3
```

Notes:

- Indexes start at **0** (the first item is `[0]`).
- `.length` gives the number of items.
- You can loop over an array with **for-in**:

```cnext
main {
    int[] nums = {10, 20, 30}
    for var n in nums {
        printin("next: " + n)
    }
}
```

Output:

```
next: 10
next: 20
next: 30
```
---

## 11. Functions

A **function** is a reusable block of code with a name. You *define* it once and
*call* it (use it) as many times as you like.

```cnext
// A function that adds two numbers and returns the result
func add(int a, int b): int {
    return a + b
}

main {
    var total = add(5, 3)   // calling the function; total becomes 8
    printin("total: " + total)
    printin("again: " + add(100, 1))
}
```

Output:

```
total: 8
again: 101
```

How to read `func add(int a, int b): int { ... }`:

- `func` says "I'm defining a function".
- `add` is its name.
- `(int a, int b)` are the **parameters** (inputs) and their types.
- `: int` is the **return type** — the type of the value it hands back.
- `return a + b` hands the result back to whoever called it.

### Short anonymous functions (lambdas)

You can also assign a small one-line function to a variable using `=>`:

```cnext
main {
    var double_it = (int x) => x * 2
    printin(double_it(21))   // 42
}
```

> **Important:** the `=>` arrow is for **lambda-style** functions assigned to a
> variable, *not* for `func` declarations. A `func` always uses `{ ... }`:

```cnext
func add(int a, int b): int {   // correct
    return a + b
}
// func add(int a, int b): int => a + b   // WRONG — this does not compile
```

---

## 12. Classes (a first taste)

A **class** is a blueprint for creating objects that have data (fields) and
behavior (methods). Don't worry if this feels advanced — skim it now and come
back later.

```cnext
class Counter {
    int count

    func new(int start) {     // this builds (constructs) a new Counter
        self.count = start    // self refers to the object we're building
    }
    func bump() {
        self.count = self.count + 1
    }
    func value(): int {
        return self.count
    }
}

main {
    Counter c = new Counter(10)   // make a Counter starting at 10
    c.bump()
    c.bump()
    printin(c.value())            // 12
}
```

---

## 13. Handling errors with try/catch

When something can go wrong (like dividing by zero), Cnext lets you *throw* an
error and *catch* it instead of crashing:

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
        printin("caught: " + err)  // handles the error gracefully
    }
}
```

Output:

```
caught: Division by zero
```

---

## 14. Put it together: a tiny interactive program

Let's combine reading input, variables, and a loop. Save as `guess.cn`:

```cnext
main {
    printin("Type your name and press Enter:")
    str name = input("name: ")
    printin("Hello, {name}!")

    var i = 0
    while i < 3 {
        printin("Countdown-ish: {i}")
        i = i + 1
    }
}
```

Run it with `cnext run guess.cn`. The program waits for you to type a name, then
prints a greeting. (If `input` isn't reading interactively on your system, type
the value before the program starts.)

---

## 15. Common beginner mistakes

- **`print` vs `printin`** — forgot the newline? Use `printin`.
- **Single vs double quotes** — `'a'` is a character, `"a"` is a string.
- **Arrow functions** — don't write `func f(...): int => ...`; that's not valid.
  Use curly braces, or assign a lambda to a variable.
- **Forgetting to update a counter** — `while` without `i = i + 1` loops forever.
- **Semicolons** — they're optional in Cnext. You can leave them out.
- **Where to put `main`** — top-level statements outside `main` aren't allowed;
  put executable code inside `main { ... }`.

---

## 16. Troubleshooting

- **`cnext: command not found`** — `cnext` isn't on your PATH. Re-check step 2.
- **`gcc not found` / compiler errors mentioning `gcc`** — Cnext needs GCC or
  Clang installed to generate the final executable. Install one and make sure
  it's on your PATH.
- **Code won't compile and shows an error with a line number** — Cnext prints the
  file, line, and a caret `^` pointing at the problem. Read that line carefully.
- **Still stuck?** See the [Syntax][] guide, open a GitHub issue in the repo, or
  ask the community.

---

## 17. Next steps — your learning path

You've done the basics. Here's a suggested path to keep learning:

1. [Syntax guide](syntax.md) — a fuller tour of the language.
2. [Types](types.md) — all the types and how to convert between them.
3. [Functions](functions.md) — deeper look at functions and parameters.
4. [Classes](classes.md) — object-oriented programming.
5. [Standard Library](stdlib.md) — files, math, strings, JSON, networking.
6. Look at the `examples/` folder for real programs:
   - `examples/hello.cn` — the hello world you already wrote
   - `examples/calculator.cn` — a small interactive calculator
   - `examples/string_processing.cn` — working with strings
7. More advanced, when you're ready: [Generics](generics.md),
   [Closures](closures.md), [Generators](generators.md),
   [Async](async.md), and [Threading](threading.md).

## Glossary

- **Compiler** — a tool that turns your source code into an executable program.
- **Function** — a named, reusable block of code.
- **Variable** — a named container that stores a value.
- **String** — text, written with double quotes.
- **Array** — an ordered list of values.
- **Loop** — a way to repeat a block of code.
- **Condition** — a true/false check used by `if` and `while`.
- **`main`** — the special function where every program starts running.

Happy coding! ✨

[Syntax]: syntax.md