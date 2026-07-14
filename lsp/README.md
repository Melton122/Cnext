# Cnext Language Server

A Language Server Protocol (LSP) implementation for the Cnext programming language.

## Features

- **Autocomplete** - Keywords, types, and built-in functions
- **Hover** - Documentation for keywords, types, and functions
- **Go to Definition** - Jump to function/class/variable definitions
- **Diagnostics** - Basic syntax error detection
- **Formatting** - Code formatting via `cnext fmt`

## Installation

### VS Code

1. Install the Cnext VS Code extension (includes LSP)
2. Or configure manually in `.vscode/settings.json`:

```json
{
  "languageserver": {
    "cnext": {
      "command": "python3",
      "args": ["path/to/cnext/lsp/server.py"],
      "filetypes": ["cnext"]
    }
  }
}
```

### Neovim (nvim-lspconfig)

```lua
require'lspconfig'.cnext.setup{
  cmd = {'python3', 'path/to/cnext/lsp/server.py'},
  filetypes = {'cnext'},
}
```

### Helix

In `~/.config/helix/languages.toml`:

```toml
[language-server.cnext]
command = "python3"
args = ["path/to/cnext/lsp/server.py"]

[[language]]
name = "cnext"
language-servers = ["cnext"]
```

### Sublime Text

Install the LSP package, then add to LSP settings:

```json
{
  "clients": {
    "cnext": {
      "command": ["python3", "path/to/cnext/lsp/server.py"],
      "selector": "source.cnext",
    }
  }
}
```

## Usage

The LSP server communicates via stdin/stdout using the Language Server Protocol.

```bash
python3 server.py
```

## Supported Languages

Works with any editor that supports LSP:
- VS Code
- Neovim
- Helix
- Zed
- Sublime Text
- Emacs (eglot)
- Atom
