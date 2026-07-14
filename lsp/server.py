#!/usr/bin/env python3
"""
Cnext Language Server Protocol (LSP) Implementation

Provides:
- Autocomplete for keywords and built-in functions
- Hover information for types and functions
- Go-to-definition
- Diagnostics (compile-time errors)
- Document formatting (via cnext fmt)
"""

import sys
import json
import subprocess
import os
import re
from typing import Optional, Dict, List, Any

# LSP protocol helpers
def read_message():
    """Read a JSON-RPC message from stdin."""
    content_length = None
    while True:
        line = sys.stdin.buffer.readline()
        if not line:
            return None
        line = line.decode('utf-8').strip()
        if line.startswith('Content-Length:'):
            content_length = int(line.split(':')[1].strip())
        elif line == '':
            break
    if content_length is None:
        return None
    body = sys.stdin.buffer.read(content_length)
    return json.loads(body.decode('utf-8'))

def send_message(msg):
    """Send a JSON-RPC message to stdout."""
    body = json.dumps(msg)
    content = f'Content-Length: {len(body.encode('utf-8'))}\r\n\r\n{body}'
    sys.stdout.buffer.write(content.encode('utf-8'))
    sys.stdout.buffer.flush()

def send_response(id, result):
    send_message({'jsonrpc': '2.0', 'id': id, 'result': result})

def send_notification(method, params):
    send_message({'jsonrpc': '2.0', 'method': method, 'params': params})

# Cnext language features
CNEXT_KEYWORDS = [
    'main', 'func', 'var', 'const', 'class', 'struct', 'enum', 'trait',
    'interface', 'if', 'else', 'while', 'for', 'in', 'return', 'break',
    'continue', 'match', 'case', 'default', 'import', 'export', 'as',
    'new', 'super', 'this', 'self', 'throw', 'try', 'catch', 'finally',
    'async', 'await', 'yield', 'coroutine', 'test', 'assert', 'macro',
    'constexpr', 'extern', 'typeof', 'own', 'extend', 'operator',
]

CNEXT_TYPES = [
    'int', 'long', 'float', 'double', 'str', 'bool', 'char', 'byte',
    'uint', 'ulong', 'ushort', 'ubyte', 'void', 'auto', 'iter',
    'Option', 'Result', 'Map', 'Set', 'List',
]

CNEXT_BUILTINS = [
    'printin', 'println', 'input', 'len', 'str', 'int', 'float',
    'bool', 'abs', 'min', 'max', 'sqrt', 'pow', 'sin', 'cos',
    'now', 'sleep', 'typeof', 'assert',
]

CNEXT_IO_BUILTINS = [
    'read_file', 'write_file', 'append_file', 'file_exists',
    'read_dir', 'create_dir', 'remove_file', 'copy_file',
]

def get_cnext_path():
    """Get the path to the cnext compiler."""
    # Try common locations
    for path in ['cnext', './cnext', '../cnext', 'cnext.exe']:
        try:
            result = subprocess.run([path, 'version'], capture_output=True, timeout=5)
            if result.returncode == 0:
                return path
        except (FileNotFoundError, subprocess.TimeoutExpired):
            continue
    return None

class CnextLSP:
    def __init__(self):
        self.root_uri = None
        self.documents: Dict[str, str] = {}
        self.cnext_path = get_cnext_path()

    def handle_initialize(self, id, params):
        self.root_uri = params.get('rootUri')
        send_response(id, {
            'capabilities': {
                'textDocumentSync': 1,  # Full sync
                'completionProvider': {
                    'triggerCharacters': ['.', '(', '"'],
                    'resolveProvider': False,
                },
                'hoverProvider': True,
                'definitionProvider': True,
                'documentFormattingProvider': True,
                'diagnosticProvider': {
                    'interFileDependencies': False,
                    'workspaceDiagnostics': False,
                },
            }
        })

    def handle_did_open(self, params):
        uri = params['textDocument']['uri']
        text = params['textDocument']['text']
        self.documents[uri] = text
        self.publish_diagnostics(uri)

    def handle_did_change(self, params):
        uri = params['textDocument']['uri']
        changes = params['contentChanges']
        if changes:
            self.documents[uri] = changes[0]['text']
        self.publish_diagnostics(uri)

    def handle_completion(self, id, params):
        uri = params['textDocument']['uri']
        position = params['position']
        text = self.documents.get(uri, '')
        line = text.split('\n')[position['line']] if position['line'] < len(text.split('\n')) else ''

        # Get word prefix
        prefix = ''
        col = position['character']
        if col > 0:
            word_chars = line[:col]
            match = re.search(r'([a-zA-Z_]\w*)$', word_chars)
            if match:
                prefix = match.group(1)

        items = []
        for kw in CNEXT_KEYWORDS:
            if not prefix or kw.startswith(prefix):
                items.append({
                    'label': kw,
                    'kind': 14,  # Keyword
                    'insertText': kw,
                })
        for tp in CNEXT_TYPES:
            if not prefix or tp.startswith(prefix):
                items.append({
                    'label': tp,
                    'kind': 22,  # TypeParameter
                    'insertText': tp,
                })
        for fn in CNEXT_BUILTINS:
            if not prefix or fn.startswith(prefix):
                items.append({
                    'label': fn,
                    'kind': 3,  # Function
                    'insertText': fn,
                })
        for fn in CNEXT_IO_BUILTINS:
            if not prefix or fn.startswith(prefix):
                items.append({
                    'label': fn,
                    'kind': 3,
                    'insertText': fn,
                })

        send_response(id, {'isIncomplete': False, 'items': items})

    def handle_hover(self, id, params):
        uri = params['textDocument']['uri']
        position = params['position']
        text = self.documents.get(uri, '')
        lines = text.split('\n')
        if position['line'] >= len(lines):
            send_response(id, None)
            return
        line = lines[position['line']]
        col = position['character']

        # Find word at cursor
        word = ''
        if col < len(line):
            match = re.search(r'([a-zA-Z_]\w*)', line[col:])
            if match:
                word = match.group(1)
            else:
                match = re.search(r'([a-zA-Z_]\w*)', line[:col][::-1])
                if match:
                    word = match.group(1)[::-1]

        contents = []
        if word in CNEXT_KEYWORDS:
            contents.append(f'**keyword** `{word}`')
            if word == 'func':
                contents.append('Function declaration: `func name(params) -> returnType { body }`')
            elif word == 'var':
                contents.append('Variable declaration: `var name = value` or `var name: Type = value`')
            elif word == 'class':
                contents.append('Class declaration: `class Name { ... }`')
            elif word == 'match':
                contents.append('Pattern matching: `match expr { pattern => result }`')
        elif word in CNEXT_TYPES:
            contents.append(f'**type** `{word}`')
        elif word in CNEXT_BUILTINS:
            contents.append(f'**builtin** `{word}()`')
        elif word in CNEXT_IO_BUILTINS:
            contents.append(f'**io** `{word}()`')
        elif re.match(r'^[A-Z]', word):
            contents.append(f'**class/struct** `{word}`')
        elif word.startswith('is_') or word.startswith('has_'):
            contents.append(f'**predicate** `{word}()` -> bool')

        if contents:
            send_response(id, {'contents': {'kind': 'markdown', 'value': '\n\n'.join(contents)}})
        else:
            send_response(id, None)

    def handle_definition(self, id, params):
        uri = params['textDocument']['uri']
        position = params['position']
        text = self.documents.get(uri, '')
        lines = text.split('\n')
        if position['line'] >= len(lines):
            send_response(id, None)
            return
        line = lines[position['line']]
        col = position['character']

        # Find word at cursor
        word = ''
        if col < len(line):
            match = re.search(r'([a-zA-Z_]\w*)', line[col:])
            if match:
                word = match.group(1)

        if not word:
            send_response(id, None)
            return

        # Search for definition in document
        for i, l in enumerate(lines):
            if word in l and ('func ' + word in l or 'class ' + word in l or
                              'struct ' + word in l or 'var ' + word in l or
                              'const ' + word in l):
                send_response(id, {
                    'uri': uri,
                    'range': {
                        'start': {'line': i, 'character': l.index(word)},
                        'end': {'line': i, 'character': l.index(word) + len(word)}
                    }
                })
                return

        send_response(id, None)

    def handle_formatting(self, id, params):
        uri = params['textDocument']['uri']
        text = self.documents.get(uri, '')

        if self.cnext_path:
            try:
                # Write temp file, format, read back
                import tempfile
                with tempfile.NamedTemporaryFile(mode='w', suffix='.cn', delete=False) as f:
                    f.write(text)
                    tmp_path = f.name
                subprocess.run([self.cnext_path, 'fmt', tmp_path], timeout=10)
                with open(tmp_path, 'r') as f:
                    formatted = f.read()
                os.unlink(tmp_path)
                if formatted != text:
                    send_response(id, [{
                        'range': {
                            'start': {'line': 0, 'character': 0},
                            'end': {'line': len(text.split('\n')), 'character': 0}
                        },
                        'newText': formatted
                    }])
                    return
            except Exception:
                pass

        send_response(id, [])

    def publish_diagnostics(self, uri):
        text = self.documents.get(uri, '')
        diagnostics = []

        lines = text.split('\n')
        for i, line in enumerate(lines):
            stripped = line.strip()
            if not stripped or stripped.startswith('//'):
                continue

            # Basic syntax checks
            if stripped.endswith('{') and not any(kw in stripped for kw in ['if', 'else', 'while', 'for', 'func', 'class', 'struct', 'enum', 'trait', 'test', 'main']):
                diagnostics.append({
                    'range': {'start': {'line': i, 'character': 0}, 'end': {'line': i, 'character': len(line)}},
                    'severity': 2,
                    'message': 'Unexpected "{" - check syntax',
                })

            # Check for common mistakes
            if '==' in stripped and '=' in stripped.replace('==', ''):
                # Could be assignment in condition
                pass

        send_notification('textDocument/publishDiagnostics', {
            'uri': uri,
            'diagnostics': diagnostics
        })

    def run(self):
        while True:
            msg = read_message()
            if msg is None:
                break

            method = msg.get('method', '')
            id = msg.get('id')
            params = msg.get('params', {})

            if method == 'initialize':
                self.handle_initialize(id, params)
            elif method == 'initialized':
                pass  # Notification, no response needed
            elif method == 'shutdown':
                send_response(id, None)
                break
            elif method == 'exit':
                break
            elif method == 'textDocument/didOpen':
                self.handle_did_open(params)
            elif method == 'textDocument/didChange':
                self.handle_did_change(params)
            elif method == 'textDocument/completion':
                self.handle_completion(id, params)
            elif method == 'textDocument/hover':
                self.handle_hover(id, params)
            elif method == 'textDocument/definition':
                self.handle_definition(id, params)
            elif method == 'textDocument/formatting':
                self.handle_formatting(id, params)
            elif id is not None:
                send_response(id, None)

if __name__ == '__main__':
    server = CnextLSP()
    server.run()
