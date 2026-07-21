# Cnext Language Support for VS Code

Syntax highlighting, snippets, linting, formatting, and build tasks for the Cnext programming language.

## Installation

### Option 1: From VSIX (Recommended)

1. Install the Cnext compiler first:
   ```bash
   # Linux/macOS
   curl -fsSL https://raw.githubusercontent.com/Melton122/cnext/main/install.sh | bash

   # Windows
   install.bat
   ```

2. Package and install the extension:
   ```bash
   cd vscode-extension
   npm install -g @vscode/vsce
   vsce package
   code --install-extension cnext-3.1.0.vsix
   ```

### Option 2: Manual Installation

1. Copy this `vscode-extension/` folder to your extensions directory:
   - **Windows:** `%USERPROFILE%\.vscode\extensions\cnext-3.1.0`
   - **macOS/Linux:** `~/.vscode/extensions/cnext-3.1.0`
2. Restart VS Code

### Option 3: From Marketplace

Search for "Cnext" in the VS Code Extensions panel (Ctrl+Shift+X).

## Features

- **Syntax Highlighting** — Full syntax highlighting for all Cnext language features
- **Code Snippets** — 25+ snippets for common patterns (classes, functions, closures, etc.)
- **Build Tasks** — Run, build, and test Cnext files with keyboard shortcuts
- **Format** — Auto-format your code with `Shift+Alt+F`
- **Lint** — Check for common issues
- **New Project** — Create a new Cnext project from VS Code
- **REPL** — Start an interactive REPL from the command palette
- **Doctor** — Check your environment for common issues

## Commands

Open the Command Palette (Ctrl+Shift+P) and type "Cnext":

| Command | Description | Shortcut |
|---------|-------------|----------|
| Cnext: Run File | Compile and run the current file | Ctrl+Shift+R |
| Cnext: Build File | Build to executable | Ctrl+Shift+B |
| Cnext: Build Release | Build optimized release | - |
| Cnext: Run Tests | Run project tests | Ctrl+Shift+T |
| Cnext: Format File | Format the current file | Shift+Alt+F |
| Cnext: Lint File | Check for issues | - |
| Cnext: New Project | Create a new project | - |
| Cnext: Doctor | Check environment | - |
| Cnext: Start REPL | Start interactive REPL | - |

## Snippets

Type a prefix and press Tab to insert:

| Prefix | Description |
|--------|-------------|
| `main` | Main block |
| `func` | Function declaration |
| `func =>` | Arrow function |
| `class` | Class declaration |
| `struct` | Struct declaration |
| `enum` | Enum declaration |
| `trait` | Trait declaration |
| `interface` | Interface declaration |
| `if` | If statement |
| `ifelse` | If-else statement |
| `while` | While loop |
| `for` | For loop |
| `forin` | For-in loop |
| `match` | Match expression |
| `try` | Try-catch block |
| `lambda` | Lambda expression |
| `generator` | Generator function |
| `async` | Async function |
| `import` | Import statement |
| `test` | Test block |

## Configuration

Open Settings (Ctrl+,) and search for "Cnext":

| Setting | Default | Description |
|---------|---------|-------------|
| `cnext.compilerPath` | `cnext` | Path to the Cnext compiler |
| `cnext.autoRun` | `false` | Automatically run file after save |
| `cnext.autoFormat` | `false` | Automatically format file after save |

## Requirements

- [Cnext compiler](https://github.com/Melton122/cnext) installed and in PATH
- GCC or Clang compiler for building Cnext programs

## Troubleshooting

### "cnext: command not found"

Set the compiler path in VS Code settings:
1. Open Settings (Ctrl+,)
2. Search for "Cnext Compiler Path"
3. Set the full path to your `cnext` executable

### Syntax highlighting not working

Make sure the file has the `.cn` extension and the language mode is set to "Cnext" (bottom-right corner of VS Code).

## Contributing

Contributions are welcome! Please see [CONTRIBUTING.md](../CONTRIBUTING.md) for guidelines.

## License

MIT License
