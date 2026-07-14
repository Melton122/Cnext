# Cnext Language Support for VS Code

Syntax highlighting, snippets, and build tasks for the Cnext programming language.

## Features

- **Syntax Highlighting** — Full syntax highlighting for Cnext language
- **Code Snippets** — Common code patterns and templates
- **Build Tasks** — Run, build, and test Cnext files
- **Auto-run** — Automatically run files on save (optional)

## Installation

### From VSIX

1. Package the extension:
   ```bash
   cd vscode-extension
   vsce package
   ```
2. Install the VSIX file in VS Code

### Manual Installation

1. Copy this folder to `~/.vscode/extensions/cnext-2.0.0`
2. Restart VS Code

## Usage

### Commands

- **Cnext: Run File** — Run the current Cnext file
- **Cnext: Build File** — Build the current Cnext file to an executable
- **Cnext: Run Tests** — Run all tests in the workspace

### Keyboard Shortcuts

- `Ctrl+Shift+R` — Run file
- `Ctrl+Shift+B` — Build file
- `Ctrl+Shift+T` — Run tests

### Snippets

Type a prefix and press Tab to insert:

- `main` — Main block
- `func` — Function declaration
- `class` — Class declaration
- `struct` — Struct declaration
- `enum` — Enum declaration
- `trait` — Trait declaration
- `interface` — Interface declaration
- `if` — If statement
- `while` — While loop
- `for` — For loop
- `forin` — For-in loop
- `switch` — Switch statement
- `match` — Match expression
- `try` — Try-catch block
- `lambda` — Lambda expression
- `generator` — Generator function
- `coroutine` — Coroutine function
- `async` — Async function
- `import` — Import statement
- `test` — Test block
- `assert` — Assert statement

## Configuration

Open Settings (Ctrl+,) and search for "Cnext":

- `cnext.compilerPath` — Path to the Cnext compiler (default: "cnext")
- `cnext.autoRun` — Automatically run file after save (default: false)

## Requirements

- Cnext compiler installed and in PATH
- GCC or Clang compiler for building

## Known Issues

- None reported

## Contributing

Contributions are welcome! Please see CONTRIBUTING.md for guidelines.

## License

MIT License
