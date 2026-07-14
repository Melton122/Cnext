# Contributing to Cnext

Thank you for your interest in contributing to Cnext! This document provides guidelines and information about contributing.

## Getting Started

1. Fork the repository
2. Clone your fork:
   ```bash
   git clone https://github.com/YOUR_USERNAME/cnext.git
   cd cnext
   ```
3. Create a feature branch:
   ```bash
   git checkout -b feature/my-feature
   ```
4. Make your changes
5. Submit a pull request

## Development Setup

### Prerequisites

- GCC or Clang compiler (C11 support)
- Make (Linux/macOS) or MinGW (Windows)
- Git
- libcurl development headers (Linux/macOS, for networking)

### Building

```bash
git clone https://github.com/Melton122/cnext.git
cd cnext

# Install libcurl (Linux)
sudo apt-get install libcurl4-openssl-dev

# Install libcurl (macOS)
brew install curl

# Build
make

# Debug build
make DEBUG=1

# Install
make install
```

### Running Tests

```bash
make test                    # Run compiler built-in tests
make check                   # Run full test suite (compiler + Python)
python3 tests/run_tests.py   # Run Python test suite directly (Linux/macOS)
```

### Code Formatting

```bash
make format   # Format source code with clang-format
```

## Project Structure

- `src/` — Compiler source code (C)
- `include/` — Header files and runtime library
- `tests/` — Test files (`.cn` test programs)
- `examples/` — Example Cnext programs
- `docs/` — Documentation
- `lsp/` — Language Server Protocol implementation
- `website/` — Registry website and playground
- `vscode-extension/` — VS Code extension

## Code Style

- **Language:** C (GNU C11 standard)
- **Indentation:** 4 spaces (no tabs)
- **Braces:** Allman style (opening brace on new line)
- **Naming:** `snake_case` for functions and variables, `UPPER_CASE` for macros
- **Header guards:** `#ifndef CNEXT_NAME_H` / `#define CNEXT_NAME_H`
- **No comments** unless the intent is non-obvious
- **Use `checked_alloc.h`** for all allocations (checked_malloc, checked_realloc, etc.)
- **Use `(void)`** for empty parameter lists in function declarations

## Adding Features

1. Create an issue describing the feature
2. Discuss the approach with maintainers
3. Implement the feature with tests
4. Update documentation if needed
5. Submit a pull request

## Adding Tests

1. Create a `.cn` file in `tests/`
2. Add expected output comments at the top:
   ```cnext
   // EXPECT: expected output line 1
   // EXPECT: expected output line 2
   ```
3. Write your test code
4. Verify it compiles and runs:
   ```bash
   cnext build tests/your_test.cn
   ./out.exe
   ```

### Test Categories

- **Lexer tests:** Token recognition
- **Parser tests:** Syntax parsing
- **Semantic tests:** Type checking
- **Codegen tests:** Code generation
- **Runtime tests:** Runtime behavior
- **Optimizer tests:** Optimization passes
- **Integration tests:** Full program tests

## Bug Reports

When reporting bugs, please include:

- Operating system and version
- GCC/Clang version (`gcc --version`)
- Cnext version (`cnext version`)
- Steps to reproduce
- Expected behavior
- Actual behavior
- Sample code if applicable

### Example Bug Report

```markdown
**Title:** Parser crashes on empty match statement

**Environment:**
- OS: Windows 11
- GCC: 13.2.0
- Cnext: v9.0

**Steps to reproduce:**
```cnext
main {
    match x {
    }
}
```

**Expected:** Parser error message
**Actual:** Segmentation fault
```

## Pull Requests

- Keep PRs focused on a single change
- Include tests for new functionality
- Update documentation as needed
- Follow the existing commit message style
- Ensure all tests pass before submitting

### PR Checklist

- [ ] Code compiles without warnings
- [ ] All tests pass (`make test`)
- [ ] New features have tests
- [ ] Documentation updated if needed
- [ ] Code follows project style

## Commit Messages

Use clear, descriptive commit messages:

- `Add feature: description` — for new features
- `Fix bug: description` — for bug fixes
- `Update docs: description` — for documentation changes
- `Refactor: description` — for code cleanup
- `Test: description` — for test additions

Examples:
```
Add feature: enum values support
Fix bug: match expression return statements
Update docs: add v9.0 to changelog
Refactor: extract string utility functions
Test: add comprehensive type system tests
```

## Development Workflow

### Making Changes

1. **Understand the issue:** Read the issue description carefully
2. **Explore the code:** Find where the change needs to be made
3. **Make minimal changes:** Only change what's necessary
4. **Test thoroughly:** Run all relevant tests
5. **Document:** Update docs if the change affects users

### Code Review

All PRs require review before merging. During review:

- Respond to feedback promptly
- Make requested changes in new commits
- Don't force-push during review
- Keep the conversation respectful

## Getting Help

- **Issues:** Use GitHub Issues for bugs and feature requests
- **Discussions:** Use GitHub Discussions for questions
- **Discord:** Join our community chat (link in README)

## First-Time Contributors

Looking for a good first issue? Look for issues labeled:
- `good first issue`
- `help wanted`
- `documentation`
- `tests`

## Code of Conduct

- Be respectful and inclusive
- Focus on constructive feedback
- Help others learn and grow
- Welcome newcomers warmly

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
