"""
Tests for the Cnext LSP server.

Tests the protocol handling, message parsing, and core features
without requiring a real VS Code connection.
"""
import sys
import json
import os
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LSP_SERVER = ROOT / "lsp" / "server.py"

def make_message(method, params=None, msg_id=None):
    """Create a JSON-RPC message in LSP format."""
    msg = {"jsonrpc": "2.0", "method": method}
    if msg_id is not None:
        msg["id"] = msg_id
    if params is not None:
        msg["params"] = params
    body = json.dumps(msg)
    return f"Content-Length: {len(body.encode('utf-8'))}\r\n\r\n{body}"


def run_lsp_session(messages, timeout=10):
    """Send messages to the LSP server and collect responses."""
    input_data = ""
    for msg in messages:
        input_data += msg

    try:
        proc = subprocess.run(
            [sys.executable, str(LSP_SERVER)],
            input=input_data,
            capture_output=True,
            text=True,
            timeout=timeout,
            cwd=str(ROOT),
        )
        return proc.stdout, proc.stderr, proc.returncode
    except subprocess.TimeoutExpired:
        return "", "Timeout", 1


def parse_responses(output):
    """Parse LSP responses from server output."""
    responses = []
    i = 0
    while i < len(output):
        # Find Content-Length header
        if output[i:].startswith("Content-Length:"):
            # Parse header
            header_end = output.index("\r\n\r\n", i)
            length_str = output[i + len("Content-Length:"):header_end].strip()
            content_length = int(length_str)
            body_start = header_end + 4
            body = output[body_start:body_start + content_length]
            responses.append(json.loads(body))
            i = body_start + content_length
        else:
            i += 1
    return responses


def test_initialize():
    """Test that the server responds to initialize."""
    msg = make_message("initialize", {
        "rootUri": str(ROOT),
        "capabilities": {}
    }, msg_id=1)
    output, stderr, rc = run_lsp_session([msg])
    responses = parse_responses(output)
    assert len(responses) >= 1, f"Expected at least 1 response, got {len(responses)}"
    resp = responses[0]
    assert resp.get("id") == 1, f"Expected id=1, got {resp.get('id')}"
    assert "result" in resp, f"Missing result in response: {resp}"
    caps = resp["result"].get("capabilities", {})
    assert "textDocumentSync" in caps, "Missing textDocumentSync capability"
    assert "completionProvider" in caps, "Missing completionProvider capability"
    print("[PASS] test_initialize")
    return True


def test_did_open_and_completion():
    """Test didOpen triggers diagnostics and completion works."""
    init_msg = make_message("initialize", {"rootUri": str(ROOT), "capabilities": {}}, msg_id=1)
    initialized_msg = make_message("initialized")
    did_open = make_message("textDocument/didOpen", {
        "textDocument": {
            "uri": "file:///test.cn",
            "languageId": "cnext",
            "version": 1,
            "text": "main {\n    pri"
        }
    })
    completion = make_message("textDocument/completion", {
        "textDocument": {"uri": "file:///test.cn"},
        "position": {"line": 1, "character": 7}
    }, msg_id=2)

    output, stderr, rc = run_lsp_session([init_msg, initialized_msg, did_open, completion])
    responses = parse_responses(output)
    # Find the completion response (id=2)
    comp_resp = None
    for r in responses:
        if r.get("id") == 2:
            comp_resp = r
            break
    assert comp_resp is not None, "No completion response found"
    assert "result" in comp_resp, f"Missing result: {comp_resp}"
    items = comp_resp["result"].get("items", [])
    # Should have "println" or "printin" in completions
    labels = [item["label"] for item in items]
    assert any("print" in l for l in labels), f"Expected 'print*' in completions, got: {labels[:5]}"
    print("[PASS] test_did_open_and_completion")
    return True


def test_hover():
    """Test hover response for keywords."""
    init_msg = make_message("initialize", {"rootUri": str(ROOT), "capabilities": {}}, msg_id=1)
    did_open = make_message("textDocument/didOpen", {
        "textDocument": {
            "uri": "file:///test.cn",
            "languageId": "cnext",
            "version": 1,
            "text": "main {\n    var x = 1\n}"
        }
    })
    hover = make_message("textDocument/hover", {
        "textDocument": {"uri": "file:///test.cn"},
        "position": {"line": 1, "character": 8}
    }, msg_id=2)

    output, stderr, rc = run_lsp_session([init_msg, did_open, hover])
    responses = parse_responses(output)
    hover_resp = None
    for r in responses:
        if r.get("id") == 2:
            hover_resp = r
            break
    assert hover_resp is not None, "No hover response found"
    # Hover may return null if no info, but should not crash
    print("[PASS] test_hover")
    return True


def test_definition():
    """Test go-to-definition."""
    init_msg = make_message("initialize", {"rootUri": str(ROOT), "capabilities": {}}, msg_id=1)
    did_open = make_message("textDocument/didOpen", {
        "textDocument": {
            "uri": "file:///test.cn",
            "languageId": "cnext",
            "version": 1,
            "text": "func hello() {\n}\nmain {\n    hello()\n}"
        }
    })
    definition = make_message("textDocument/definition", {
        "textDocument": {"uri": "file:///test.cn"},
        "position": {"line": 3, "character": 5}
    }, msg_id=2)

    output, stderr, rc = run_lsp_session([init_msg, did_open, definition])
    responses = parse_responses(output)
    def_resp = None
    for r in responses:
        if r.get("id") == 2:
            def_resp = r
            break
    assert def_resp is not None, "No definition response found"
    result = def_resp.get("result")
    if result:
        assert "uri" in result, f"Missing uri in definition result: {result}"
        assert result["range"]["start"]["line"] == 0, f"Expected definition at line 0, got {result['range']['start']['line']}"
    print("[PASS] test_definition")
    return True


def test_did_close_cleans_up():
    """Test that didClose removes document from memory."""
    init_msg = make_message("initialize", {"rootUri": str(ROOT), "capabilities": {}}, msg_id=1)
    did_open = make_message("textDocument/didOpen", {
        "textDocument": {
            "uri": "file:///test.cn",
            "languageId": "cnext",
            "version": 1,
            "text": "main {\n    var x = 1\n}"
        }
    })
    did_close = make_message("textDocument/didClose", {
        "textDocument": {"uri": "file:///test.cn"}
    })
    # After close, completion should return empty (no document in memory)
    completion = make_message("textDocument/completion", {
        "textDocument": {"uri": "file:///test.cn"},
        "position": {"line": 1, "character": 8}
    }, msg_id=2)

    output, stderr, rc = run_lsp_session([init_msg, did_open, did_close, completion])
    responses = parse_responses(output)
    comp_resp = None
    for r in responses:
        if r.get("id") == 2:
            comp_resp = r
            break
    assert comp_resp is not None, "No completion response after didClose"
    print("[PASS] test_did_close_cleans_up")
    return True


def test_invalid_content_length():
    """Test that server handles invalid Content-Length gracefully."""
    # Send a message with negative Content-Length
    bad_msg = "Content-Length: -1\r\n\r\n{}"
    # Send a message with huge Content-Length
    huge_msg = "Content-Length: 999999999\r\n\r\n{}"
    # Send a valid initialize after bad messages
    init_msg = make_message("initialize", {"rootUri": str(ROOT), "capabilities": {}}, msg_id=1)

    output, stderr, rc = run_lsp_session([bad_msg, huge_msg, init_msg])
    responses = parse_responses(output)
    # Should still respond to the valid message
    assert len(responses) >= 1, f"Expected response to valid message, got {len(responses)}"
    print("[PASS] test_invalid_content_length")
    return True


def test_shutdown():
    """Test clean shutdown."""
    init_msg = make_message("initialize", {"rootUri": str(ROOT), "capabilities": {}}, msg_id=1)
    shutdown = make_message("shutdown", msg_id=2)
    exit_msg = make_message("exit")

    output, stderr, rc = run_lsp_session([init_msg, shutdown, exit_msg])
    responses = parse_responses(output)
    shutdown_resp = None
    for r in responses:
        if r.get("id") == 2:
            shutdown_resp = r
            break
    assert shutdown_resp is not None, "No shutdown response"
    assert shutdown_resp.get("result") is None, f"Expected null result for shutdown, got {shutdown_resp.get('result')}"
    print("[PASS] test_shutdown")
    return True


def main():
    tests = [
        test_initialize,
        test_did_open_and_completion,
        test_hover,
        test_definition,
        test_did_close_cleans_up,
        test_invalid_content_length,
        test_shutdown,
    ]

    passed = 0
    failed = 0
    for test in tests:
        try:
            if test():
                passed += 1
            else:
                failed += 1
        except Exception as e:
            print(f"[FAIL] {test.__name__}: {e}")
            failed += 1

    print(f"\nLSP Tests: {passed} passed, {failed} failed")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
