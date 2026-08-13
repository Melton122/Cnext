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
import threading
import time
from typing import Optional, Dict, List, Any

# LSP protocol helpers
MAX_CONTENT_LENGTH = 10 * 1024 * 1024  # 10MB limit

# Buffered stdin reader with push-back so we can resynchronize after a
# malformed/oversized frame instead of dropping subsequent valid messages.
_MARKER = b'Content-Length:'
_buf = bytearray()


def _fill():
    """Read more into the internal buffer. Return False on EOF."""
    chunk = sys.stdin.buffer.read(4096)
    if not chunk:
        return False
    _buf.extend(chunk)
    return True


def _read_line():
    """Return one line (without the trailing newline), or None on EOF."""
    while True:
        idx = _buf.find(b'\n')
        if idx != -1:
            line = bytes(_buf[:idx])
            del _buf[:idx + 1]
            return line
        if not _fill():
            if _buf:
                line = bytes(_buf)
                _buf.clear()
                return line
            return None


def _read_exact(n):
    """Read exactly n bytes from the buffer, or None on premature EOF."""
    while len(_buf) < n:
        if not _fill():
            return None
    data = bytes(_buf[:n])
    del _buf[:n]
    return data


def _discard_until_next_header():
    """Discard bytes up to (but not including) the next 'Content-Length:' header."""
    while True:
        idx = _buf.find(_MARKER)
        if idx != -1:
            del _buf[:idx]
            return True
        if not _fill():
            return False


def read_message():
    """Read a JSON-RPC message from stdin. Returns None on EOF.

    Corrupt frames (invalid or oversized Content-Length, bad JSON body) are
    skipped and the next valid message is still processed.
    """
    while True:
        content_length = None
        while True:
            line = _read_line()
            if line is None:
                return None
            line = line.decode('utf-8', 'replace').strip()
            if line.startswith('Content-Length:'):
                try:
                    content_length = int(line.split(':', 1)[1].strip())
                except (ValueError, IndexError):
                    content_length = None
            elif line == '':
                break

        if content_length is not None and not (content_length < 0 or content_length > MAX_CONTENT_LENGTH):
            body = _read_exact(content_length)
            if body is None:
                return None
            try:
                return json.loads(body.decode('utf-8'))
            except (ValueError, UnicodeDecodeError):
                # Valid length but malformed body: skip and keep serving.
                continue

        # Invalid or oversized frame: drop its body and resync on the next header.
        if not _discard_until_next_header():
            return None

_send_lock = threading.Lock()

def send_message(msg):
    """Send a JSON-RPC message to stdout (thread-safe)."""
    body = json.dumps(msg)
    data = f'Content-Length: {len(body.encode("utf-8"))}\r\n\r\n{body}'.encode('utf-8')
    with _send_lock:
        sys.stdout.buffer.write(data)
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
    'abstract', 'bench', 'channel', 'defer', 'extends', 'final', 'iter',
    'lock', 'mutex', 'none', 'override', 'recv', 'resume', 'run_async',
    'send', 'spawn', 'static', 'thread', 'type', 'unlock', 'when', 'with',
]

CNEXT_TYPES = [
    'int', 'long', 'float', 'double', 'str', 'bool', 'char', 'byte',
    'uint', 'ulong', 'ushort', 'ubyte', 'void', 'auto', 'iter',
    'Option', 'Result', 'Map', 'Set', 'List',
]

CNEXT_BUILTINS = [
    'printin', 'println', 'print', 'input', 'len', 'str', 'int', 'float',
    'bool', 'to_int', 'to_float', 'to_str', 'to_bool', 'to_char',
    'parse_int', 'parse_float', 'char', 'ord', 'bytes', 'assert', 'typeof',
    'free', 'exit', 'panic', 'gc', 'stacktrace', 'debug', 'benchmark',
    'clone', 'swap_values', 'sort', 'push', 'pop', 'shift', 'unshift',
    'insert_at', 'remove_at', 'clear', 'contains_item', 'reverse_array',
    'array_index', 'array_first', 'array_last', 'array_slice',
    'array_unique', 'array_shuffle', 'array_map', 'array_filter',
    'array_reduce', 'array_find',
    'md5', 'sha1', 'sha256', 'uuid',
    'base64_encode', 'base64_decode',
]

CNEXT_STRING_BUILTINS = [
    'str_upper', 'str_lower', 'str_to_upper', 'str_to_lower', 'str_trim',
    'str_split', 'str_join', 'str_replace', 'str_contains',
    'str_starts_with', 'str_ends_with', 'str_substring', 'str_sub_chars',
    'str_index_of', 'str_last_index_of', 'str_char_at', 'str_char_count',
    'str_codepoint_at', 'str_find', 'str_reverse', 'str_repeat',
    'str_capitalize', 'str_title', 'str_pad_left', 'str_pad_right',
    'str_remove', 'str_insert', 'str_to_int', 'str_to_float', 'str_count',
    'str_is_empty',
]

CNEXT_MATH_BUILTINS = [
    'math_abs', 'math_min', 'math_max', 'math_clamp', 'math_clamp_int',
    'math_round', 'math_floor', 'math_ceil', 'math_sqrt', 'math_pow',
    'math_log', 'math_log10', 'math_logb', 'math_exp', 'math_sin',
    'math_cos', 'math_tan', 'math_atan', 'math_sinh', 'math_fmod',
    'math_lerp', 'math_random', 'math_random_range', 'math_random_int',
    'math_gcd', 'math_lcm', 'math_factorial', 'math_fibonacci',
]

CNEXT_TIME_BUILTINS = [
    'time_now', 'time_sleep', 'time_timestamp', 'time_date', 'time_time',
    'time_stopwatch_start', 'time_stopwatch_stop', 'time_format_time',
]

CNEXT_IO_BUILTINS = [
    'read_file', 'write_file', 'append_file', 'file_exists', 'file_size',
    'read_dir', 'create_dir', 'remove_file', 'copy_file', 'move_file',
    'delete_file',
]

CNEXT_SYSTEM_BUILTINS = [
    'cwd', 'chdir', 'platform', 'sys_args', 'sys_args_count', 'sys_arg_at',
    'getenv', 'setenv', 'hostname', 'username', 'cpu_count',
    'memory_usage', 'disk_usage', 'temp_dir', 'home_dir', 'sys_exec',
    'sys_shell', 'json_parse', 'json_stringify',
]

CNEXT_MEMORY_BUILTINS = [
    'mem_arena_create', 'mem_arena_alloc', 'mem_arena_free',
    'mem_arena_destroy', 'mem_arena_usage', 'mem_scope_begin',
    'mem_scope_end', 'mem_scope_usage', 'ref_new', 'ref_retain',
    'ref_release', 'ref_count',
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

ANSI_RE = re.compile(r'\x1b\[[0-9;]*m')


def _strip_ansi(text):
    return ANSI_RE.sub('', text)


# Matches compiler diagnostics like "error [E005] [3:13]: Unexpected ..."
CNEXT_DIAG_RE = re.compile(
    r'\b(error|warning)\b.*\[\s*(\d+)(?::(\d+))?\]:\s*(.*)$'
)


def _parse_compiler_output(output):
    """Extract (line_number, column, message, is_error) tuples from `cnext build` output."""
    results = []
    for raw_line in output.splitlines():
        line = _strip_ansi(raw_line)
        m = CNEXT_DIAG_RE.search(line)
        if not m:
            continue
        kind, lineno, colno, message = m.group(1), m.group(2), m.group(3), m.group(4)
        if kind == 'error':
            results.append((int(lineno), int(colno) if colno else 0, message, True))
        elif kind == 'warning':
            results.append((int(lineno), int(colno) if colno else 0, message, False))
    return results


class CnextLSP:
    def __init__(self):
        self.root_uri = None
        self.documents: Dict[str, str] = {}
        self.cnext_path = get_cnext_path()
        self._publish_uris = set()
        self._publish_lock = threading.Lock()
        self._publish_timer: Optional[threading.Timer] = None

    def schedule_diagnostics(self, uri):
        """Debounce diagnostics: compile at most once per 0.6s window."""
        with self._publish_lock:
            self._publish_uris.add(uri)
            if self._publish_timer is None:
                self._publish_timer = threading.Timer(0.6, self._flush_diagnostics)
                self._publish_timer.daemon = True
                self._publish_timer.start()

    def _flush_diagnostics(self):
        with self._publish_lock:
            self._publish_timer = None
            uris = list(self._publish_uris)
            self._publish_uris.clear()
        for uri in uris:
            if uri in self.documents:
                try:
                    self.publish_diagnostics(uri)
                except Exception as e:
                    sys.stderr.write(f'Diagnostics error for {uri}: {e}\n')

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
        self.schedule_diagnostics(uri)

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
                    'kind': 22,  # Struct
                    'insertText': tp,
                })
        groups = [
            (CNEXT_BUILTINS, 3),
            (CNEXT_STRING_BUILTINS, 3),
            (CNEXT_MATH_BUILTINS, 3),
            (CNEXT_TIME_BUILTINS, 3),
            (CNEXT_IO_BUILTINS, 3),
            (CNEXT_SYSTEM_BUILTINS, 3),
            (CNEXT_MEMORY_BUILTINS, 3),
        ]
        seen = set()
        for group, kind in groups:
            for fn in group:
                if fn in seen:
                    continue
                seen.add(fn)
                if not prefix or fn.startswith(prefix):
                    items.append({
                        'label': fn,
                        'kind': kind,
                        'insertText': fn,
                        'detail': 'Cnext builtin',
                    })

        # Identifiers declared in this document, for local completion.
        doc_ids = sorted(set(re.findall(r'\b([a-zA-Z_][a-zA-Z0-9_]*)\b', text)))
        for ident in doc_ids:
            if ident in CNEXT_KEYWORDS or ident in CNEXT_TYPES or ident in seen:
                continue
            if not prefix or ident.startswith(prefix):
                items.append({
                    'label': ident,
                    'kind': 6,  # Variable
                    'insertText': ident,
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
            elif word == 'import':
                contents.append('Imports a module: `import io`')
            elif word == 'constexpr':
                contents.append('Compile-time constant evaluation')
            elif word == 'own':
                contents.append('Ownership marker for memory management')
        elif word in CNEXT_TYPES:
            contents.append(f'**type** `{word}`')
        elif word in CNEXT_STRING_BUILTINS:
            contents.append(f'**string builtin** `{word}()`')
        elif word in CNEXT_MATH_BUILTINS:
            contents.append(f'**math builtin** `{word}()`')
        elif word in CNEXT_TIME_BUILTINS:
            contents.append(f'**time builtin** `{word}()`')
        elif word in CNEXT_SYSTEM_BUILTINS:
            contents.append(f'**system builtin** `{word}()`')
        elif word in CNEXT_MEMORY_BUILTINS:
            contents.append(f'**memory builtin** `{word}()`')
        elif word in CNEXT_BUILTINS or word in CNEXT_IO_BUILTINS:
            contents.append(f'**builtin** `{word}()`')
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
        tmp_path = None

        if self.cnext_path:
            try:
                import tempfile
                with tempfile.NamedTemporaryFile(mode='w', suffix='.cn', delete=False) as f:
                    f.write(text)
                    tmp_path = f.name
                subprocess.run([self.cnext_path, 'fmt', tmp_path], timeout=10)
                with open(tmp_path, 'r') as f:
                    formatted = f.read()
                if formatted != text:
                    send_response(id, [{
                        'range': {
                            'start': {'line': 0, 'character': 0},
                            'end': {'line': len(text.split('\n')), 'character': 0}
                        },
                        'newText': formatted
                    }])
                    return
            except (subprocess.TimeoutExpired, OSError, IOError) as e:
                sys.stderr.write(f'Formatting error: {e}\n')
            finally:
                if tmp_path:
                    try:
                        os.unlink(tmp_path)
                    except OSError:
                        pass

        send_response(id, [])

    def publish_diagnostics(self, uri):
        text = self.documents.get(uri, '')
        if self.cnext_path is None:
            send_notification('textDocument/publishDiagnostics', {
                'uri': uri,
                'diagnostics': [],
            })
            return

        tmp_path = None
        diagnostics = []
        try:
            import tempfile
            with tempfile.NamedTemporaryFile(
                    mode='w', suffix='.cn', delete=False,
                    encoding='utf-8', newline='') as f:
                f.write(text)
                tmp_path = f.name

            proc = subprocess.run(
                [self.cnext_path, 'build', tmp_path, '-o', tmp_path + '.out'],
                capture_output=True, timeout=15,
                cwd=os.path.dirname(tmp_path) or None,
                text=True, encoding='utf-8', errors='replace',
            )
            output = (proc.stdout or '') + (proc.stderr or '')
            doc_lines = text.split('\n')
            for lineno, colno, message, is_error in _parse_compiler_output(output):
                # Compiler line numbers are 1-based.
                idx = max(0, lineno - 1)
                line_len = len(doc_lines[idx]) if idx < len(doc_lines) else 0
                start_char = max(0, colno - 1) if colno else 0
                diagnostics.append({
                    'range': {
                        'start': {'line': idx, 'character': start_char},
                        'end': {'line': idx, 'character': max(start_char + 1, line_len)},
                    },
                    'severity': 1 if is_error else 2,
                    'message': message,
                    'source': 'cnext',
                })
        except subprocess.TimeoutExpired:
            pass
        except (OSError, IOError) as e:
            sys.stderr.write(f'Diagnostics error: {e}\n')
        finally:
            if tmp_path:
                try:
                    os.unlink(tmp_path)
                except OSError:
                    pass
                try:
                    os.unlink(tmp_path + '.out')
                except OSError:
                    pass

        send_notification('textDocument/publishDiagnostics', {
            'uri': uri,
            'diagnostics': diagnostics,
        })

    def handle_did_close(self, params):
        uri = params['textDocument']['uri']
        self.documents.pop(uri, None)
        with self._publish_lock:
            self._publish_uris.discard(uri)
        send_notification('textDocument/publishDiagnostics', {
            'uri': uri,
            'diagnostics': [],
        })

    def run(self):
        while True:
            msg = read_message()
            if msg is None:
                break

            method = msg.get('method', '')
            id = msg.get('id')
            params = msg.get('params', {})

            try:
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
                elif method == 'textDocument/didClose':
                    self.handle_did_close(params)
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
            except Exception as e:
                sys.stderr.write(f'Error handling {method}: {e}\n')
                if id is not None:
                    try:
                        send_response(id, None)
                    except Exception:
                        pass

if __name__ == '__main__':
    server = CnextLSP()
    server.run()
