const vscode = require('vscode');
const { execFile, execFileSync, spawn } = require('child_process');
const path = require('path');
const fs = require('fs');
const util = require('util');
const execFilePromise = util.promisify(execFile);

let outputChannel;

const LSP_COMPLETION_KINDS = {
    1: vscode.CompletionItemKind.Text,
    2: vscode.CompletionItemKind.Method,
    3: vscode.CompletionItemKind.Function,
    4: vscode.CompletionItemKind.Constructor,
    5: vscode.CompletionItemKind.Field,
    6: vscode.CompletionItemKind.Variable,
    7: vscode.CompletionItemKind.Class,
    8: vscode.CompletionItemKind.Interface,
    9: vscode.CompletionItemKind.Module,
    10: vscode.CompletionItemKind.Property,
    12: vscode.CompletionItemKind.Value,
    13: vscode.CompletionItemKind.Enum,
    14: vscode.CompletionItemKind.Keyword,
    15: vscode.CompletionItemKind.Snippet,
    18: vscode.CompletionItemKind.Reference,
    20: vscode.CompletionItemKind.EnumMember,
    21: vscode.CompletionItemKind.Constant,
    22: vscode.CompletionItemKind.Struct,
    23: vscode.CompletionItemKind.Event,
    24: vscode.CompletionItemKind.Operator,
    25: vscode.CompletionItemKind.TypeParameter
};

const LSP_DIAG_SEVERITIES = {
    1: vscode.DiagnosticSeverity.Error,
    2: vscode.DiagnosticSeverity.Warning,
    3: vscode.DiagnosticSeverity.Information,
    4: vscode.DiagnosticSeverity.Hint
};

class LspClient {
    constructor() {
        this.diagnostics = vscode.languages.createDiagnosticCollection('cnext');
        this.pending = new Map();
        this.nextId = 1;
        this.process = null;
        this.state = 'stopped';
        this.buffer = Buffer.alloc(0);
        this.python = null;
        this.serverPath = null;
    }

    get running() {
        return this.state === 'running';
    }

    async start() {
        await this.stop();
        this.diagnostics.clear();
        const config = vscode.workspace.getConfiguration('cnext');
        if (!config.get('lsp.enable', true)) {
            this.setStatus('$(rocket) Cnext (LSP disabled)');
            return;
        }

        this.python = await this.resolvePython(config.get('lsp.pythonPath', ''));
        if (!this.python) {
            this.setStatus('$(rocket) Cnext (install Python for LSP)');
            outputChannel.appendLine('[lsp] Python interpreter not found (tried python, python3, py). Rich features disabled.');
            return;
        }
        this.serverPath = this.resolveServerPath(config.get('lsp.serverPath', ''));
        if (!this.serverPath) {
            this.setStatus('$(rocket) Cnext (lsp/server.py missing)');
            outputChannel.appendLine('[lsp] Could not locate lsp/server.py. Rich features disabled.');
            return;
        }

        try {
            this.process = spawn(this.python, [this.serverPath], {
                stdio: ['pipe', 'pipe', 'pipe'],
                windowsHide: true
            });
        } catch (err) {
            outputChannel.appendLine(`[lsp] Failed to spawn: ${err.message}`);
            this.setStatus('$(rocket) Cnext (LSP failed)');
            return;
        }

        this.state = 'starting';
        this.process.stdout.on('data', (chunk) => this.onData(chunk));
        this.process.stderr.on('data', (chunk) => {
            const text = chunk.toString();
            if (text.trim()) outputChannel.appendLine(`[lsp] ${text.trim()}`);
        });
        this.process.on('error', (err) => {
            outputChannel.appendLine(`[lsp] Process error: ${err.message}`);
            if (this.state !== 'stopped') {
                this.state = 'stopped';
                this.diagnostics.clear();
                this.setStatus('$(rocket) Cnext (LSP error)');
            }
        });
        this.process.on('exit', (code) => {
            this.rejectAll('LSP exited');
            if (this.state !== 'stopped') {
                this.state = 'stopped';
                this.diagnostics.clear();
                this.setStatus(`$(rocket) Cnext (LSP exited: ${code})`);
            }
        });

        try {
            await this.request('initialize', {
                rootUri: this.rootUri(),
                initializationOptions: { compilerPath: config.get('compilerPath', 'cnext') },
                capabilities: {
                    textDocument: { synchronization: { didSave: true } }
                }
            }, 10000);
            this.notify('initialized', {});
            this.state = 'running';
            this.setStatus('$(rocket) Cnext');
            outputChannel.appendLine('[lsp] Language server ready.');
            this.syncOpenDocuments();
        } catch (err) {
            this.state = 'stopped';
            this.setStatus('$(rocket) Cnext (LSP failed)');
            outputChannel.appendLine(`[lsp] Initialize failed: ${err.message}`);
        }
    }

    async stop() {
        if (this.process) {
            const proc = this.process;
            this.process = null;
            this.state = 'stopped';
            this.rejectAll('LSP stopped');
            try {
                proc.stdin.end();
            } catch (e) { /* ignore */ }
            setTimeout(() => {
                try { proc.kill(); } catch (e) { /* ignore */ }
            }, 300);
        }
    }

    dispose() {
        this.stop();
        this.diagnostics.dispose();
    }

    rootUri() {
        const folder = vscode.workspace.workspaceFolders?.[0];
        return folder ? vscode.Uri.file(folder.uri.fsPath).toString() : null;
    }

    resolvePython(configured) {
        const candidates = configured ? [configured] : ['python', 'python3', 'py'];
        for (const candidate of candidates) {
            try {
                execFileSync(candidate, ['--version'], { timeout: 3000, stdio: 'ignore' });
                return candidate;
            } catch (e) { /* try next */ }
        }
        return null;
    }

    resolveServerPath(configured) {
        if (configured && fs.existsSync(configured)) return configured;
        const bundled = path.join(__dirname, 'lsp', 'server.py');
        if (fs.existsSync(bundled)) return bundled;
        const folder = vscode.workspace.workspaceFolders?.[0];
        if (folder) {
            const workspace = path.join(folder.uri.fsPath, 'lsp', 'server.py');
            if (fs.existsSync(workspace)) return workspace;
        }
        return null;
    }

    setStatus(text) {
        actStatusBar.text = text;
    }

    onData(chunk) {
        this.buffer = Buffer.concat([this.buffer, chunk]);
        for (;;) {
            const headerEnd = this.buffer.indexOf('\r\n\r\n');
            if (headerEnd === -1) {
                if (this.buffer.length > 65536) this.buffer = Buffer.alloc(0);
                return;
            }
            const header = this.buffer.slice(0, headerEnd).toString('utf8');
            const match = /Content-Length:\s*(\d+)/i.exec(header);
            this.buffer = this.buffer.slice(headerEnd + 4);
            if (!match) continue;
            const length = parseInt(match[1], 10);
            if (length < 0 || length > 10 * 1024 * 1024) continue;
            if (this.buffer.length < length) return;
            const body = this.buffer.slice(0, length);
            this.buffer = this.buffer.slice(length);
            try {
                this.handleMessage(JSON.parse(body.toString('utf8')));
            } catch (err) {
                outputChannel.appendLine(`[lsp] Invalid message: ${err.message}`);
            }
        }
    }

    sendRaw(message) {
        if (!this.process || !this.process.stdin.writable || this.state === 'stopped') return false;
        const body = Buffer.from(JSON.stringify(message), 'utf8');
        try {
            this.process.stdin.write(`Content-Length: ${body.length}\r\n\r\n`);
            this.process.stdin.write(body);
            return true;
        } catch (err) {
            outputChannel.appendLine(`[lsp] Write failed: ${err.message}`);
            return false;
        }
    }

    request(method, params, timeoutMs) {
        return new Promise((resolve, reject) => {
            if (!this.process) {
                reject(new Error('Language server is not running'));
                return;
            }
            const id = this.nextId++;
            const timer = setTimeout(() => {
                this.pending.delete(id);
                reject(new Error(`LSP timeout: ${method}`));
            }, timeoutMs || 8000);
            this.pending.set(id, { resolve, reject, timer });
            if (!this.sendRaw({ jsonrpc: '2.0', id, method, params: params || {} })) {
                clearTimeout(timer);
                this.pending.delete(id);
                reject(new Error('Language server is not running'));
            }
        });
    }

    notify(method, params) {
        this.sendRaw({ jsonrpc: '2.0', method, params: params || {} });
    }

    handleMessage(message) {
        if (message && typeof message.id === 'number' && this.pending.has(message.id)) {
            const entry = this.pending.get(message.id);
            this.pending.delete(message.id);
            clearTimeout(entry.timer);
            if (message.error) {
                entry.reject(new Error(message.error.message || 'LSP error'));
            } else {
                entry.resolve(message.result);
            }
            return;
        }
        if (message && message.method === 'textDocument/publishDiagnostics') {
            this.applyDiagnostics(message.params);
        }
    }

    rejectAll(reason) {
        for (const entry of this.pending.values()) {
            clearTimeout(entry.timer);
            entry.reject(new Error(reason));
        }
        this.pending.clear();
    }

    applyDiagnostics(params) {
        const uri = params && params.uri;
        if (!uri) return;
        const diags = ((params && params.diagnostics) || []).map((d) => {
            const r = d.range || {};
            const start = r.start || { line: 0, character: 0 };
            const end = r.end || start;
            const range = new vscode.Range(start.line, start.character, end.line, end.character);
            const severity = LSP_DIAG_SEVERITIES[d.severity] || vscode.DiagnosticSeverity.Error;
            const diag = new vscode.Diagnostic(range, d.message || 'Cnext issue', severity);
            if (d.code) diag.code = d.code;
            if (d.source) diag.source = d.source;
            return diag;
        });
        try {
            this.diagnostics.set(vscode.Uri.parse(uri), diags);
        } catch (err) {
            outputChannel.appendLine(`[lsp] Bad diagnostics URI: ${err.message}`);
        }
    }

    lspPosition(position) {
        return { line: position.line, character: position.character };
    }

    lspRange(range) {
        return {
            start: this.lspPosition(range.start),
            end: this.lspPosition(range.end)
        };
    }

    toVscodeRange(lspRangeValue) {
        return new vscode.Range(
            lspRangeValue.start.line, lspRangeValue.start.character,
            lspRangeValue.end.line, lspRangeValue.end.character
        );
    }

    syncOpenDocuments() {
        for (const document of vscode.workspace.textDocuments) {
            if (document.languageId === 'cnext') {
                this.notify('textDocument/didOpen', this.documentParams(document));
            }
        }
    }

    documentParams(document) {
        return {
            textDocument: {
                uri: document.uri.toString(),
                languageId: 'cnext',
                version: 1,
                text: document.getText()
            }
        };
    }

    documentId(document) {
        return { textDocument: { uri: document.uri.toString() } };
    }

    async completion(uri, position) {
        const result = await this.request('textDocument/completion', {
            textDocument: { uri },
            position: this.lspPosition(position)
        });
        if (!result || !result.items) return [];
        return result.items.map((item) => {
            const completion = new vscode.CompletionItem(
                item.label,
                LSP_COMPLETION_KINDS[item.kind] || vscode.CompletionItemKind.Text
            );
            if (item.insertText) completion.insertText = item.insertText;
            if (item.detail) completion.detail = item.detail;
            if (item.documentation) completion.documentation = item.documentation;
            if (item.sortText) completion.sortText = item.sortText;
            return completion;
        });
    }

    async hover(uri, position) {
        const result = await this.request('textDocument/hover', {
            textDocument: { uri },
            position: this.lspPosition(position)
        });
        if (!result || !result.contents) return null;
        let value = '';
        if (typeof result.contents === 'string') {
            value = result.contents;
        } else if (Array.isArray(result.contents)) {
            value = result.contents.map((c) => (typeof c === 'string' ? c : c.value || '')).join('\n\n');
        } else if (result.contents.value) {
            value = result.contents.value;
        }
        if (!value) return null;
        const markdown = new vscode.MarkdownString(value);
        markdown.supportHtml = false;
        const range = result.range ? this.toVscodeRange(result.range) : undefined;
        return new vscode.Hover(markdown, range);
    }

    async definition(uri, position) {
        const result = await this.request('textDocument/definition', {
            textDocument: { uri },
            position: this.lspPosition(position)
        });
        if (!result) return null;
        return new vscode.Location(
            vscode.Uri.parse(result.uri),
            this.toVscodeRange(result.range)
        );
    }

    async formatting(uri, text) {
        const result = await this.request('textDocument/formatting', {
            textDocument: { uri },
            options: { tabSize: 4, insertSpaces: true }
        });
        if (!result || !result.length) return [];
        return result.map((edit) => new vscode.TextEdit(this.toVscodeRange(edit.range), edit.newText));
    }
}

let statusBar;
let lsp;

async function activateClient() {
    await lsp.start();
}

function registerCommand(context, name, handler, title) {
    context.subscriptions.push(vscode.commands.registerCommand(name, handler));
    return name;
}

function activate(context) {
    outputChannel = vscode.window.createOutputChannel('Cnext');
    outputChannel.appendLine('Cnext extension activated');

    lsp = new LspClient();
    statusBar = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 100);
    statusBar.text = '$(rocket) Cnext';
    statusBar.tooltip = 'Cnext language support — click for compiler environment check';
    statusBar.command = 'cnext.doctor';
    statusBar.show();
    lsp.setStatus = (text) => { statusBar.text = text; };

    function getCompilerPath() {
        const config = vscode.workspace.getConfiguration('cnext');
        return config.get('compilerPath', 'cnext');
    }

    function getCwd() {
        const workspaceFolder = vscode.workspace.workspaceFolders?.[0];
        return workspaceFolder ? workspaceFolder.uri.fsPath : undefined;
    }

    async function runCommand(command, argsArray, cwd) {
        const compiler = getCompilerPath();
        const fullArgs = [command, ...(argsArray || [])];
        outputChannel.appendLine(`> ${compiler} ${fullArgs.join(' ')}`);

        try {
            const { stdout, stderr } = await execFilePromise(compiler, fullArgs, { cwd: cwd || getCwd(), timeout: 30000 });
            if (stdout) outputChannel.appendLine(stdout);
            if (stderr) outputChannel.appendLine(stderr);
            outputChannel.show(true);
            return { success: true, output: stdout };
        } catch (error) {
            outputChannel.appendLine(`Error: ${error.message}`);
            if (error.stdout) outputChannel.appendLine(error.stdout);
            if (error.stderr) outputChannel.appendLine(error.stderr);
            outputChannel.show(true);
            return { success: false, output: error.message };
        }
    }

    function isCnextFile(document) {
        return document && document.languageId === 'cnext';
    }

    async function withCnextDocument(fn) {
        const editor = vscode.window.activeTextEditor;
        if (!editor || !isCnextFile(editor.document)) {
            vscode.window.showErrorMessage('Open a .cn file first');
            return null;
        }
        await editor.document.save();
        return fn(editor.document);
    }

    // --- Compiler commands ---
    registerCommand(context, 'cnext.run', async () => {
        withCnextDocument(async (doc) => {
            const result = await runCommand('run', [doc.fileName]);
            if (result.success && result.output.trim()) {
                vscode.window.showInformationMessage(`Output: ${result.output.trim().split('\n')[0]}`);
            }
        });
    });

    registerCommand(context, 'cnext.build', async () => {
        withCnextDocument(async (doc) => {
            const ext = process.platform === 'win32' ? '.exe' : '';
            const outputFile = doc.fileName.replace(/\.cn$/, ext);
            const result = await runCommand('build', [doc.fileName, '-o', outputFile]);
            if (result.success) {
                vscode.window.showInformationMessage(`Built: ${path.basename(outputFile)}`);
            }
        });
    });

    registerCommand(context, 'cnext.buildRelease', async () => {
        withCnextDocument(async (doc) => {
            const ext = process.platform === 'win32' ? '.exe' : '';
            const outputFile = doc.fileName.replace(/\.cn$/, ext);
            const result = await runCommand('build', [doc.fileName, '-o', outputFile, '--release']);
            if (result.success) {
                vscode.window.showInformationMessage(`Release built: ${path.basename(outputFile)}`);
            }
        });
    });

    registerCommand(context, 'cnext.test', async () => {
        await runCommand('test', []);
    });

    registerCommand(context, 'cnext.format', async () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor || !isCnextFile(editor.document)) {
            vscode.window.showErrorMessage('Open a .cn file first');
            return;
        }
        await editor.document.save();
        const result = await runCommand('fmt', [editor.document.fileName]);
        if (result.success) {
            vscode.window.showInformationMessage('Formatted');
            await vscode.commands.executeCommand('workbench.action.revertAndCloseActiveEditor');
            await vscode.window.showTextDocument(vscode.Uri.file(editor.document.fileName));
        }
    });

    registerCommand(context, 'cnext.lint', async () => {
        await withCnextDocument(async (doc) => {
            await runCommand('lint', [doc.fileName]);
        });
    });

    registerCommand(context, 'cnext.newProject', async () => {
        const name = await vscode.window.showInputBox({
            prompt: 'Project name',
            placeHolder: 'my_project',
            validateInput: (value) => {
                if (!value || !/^[a-zA-Z_][a-zA-Z0-9_]*$/.test(value)) {
                    return 'Project name must start with a letter or underscore and contain only letters, numbers, and underscores';
                }
                return null;
            }
        });
        if (!name) return;
        const result = await runCommand('new', [name]);
        if (result.success) {
            vscode.window.showInformationMessage(`Project "${name}" created. Open the folder to start coding!`);
        }
    });

    registerCommand(context, 'cnext.doctor', async () => {
        await runCommand('doctor', []);
    });

    registerCommand(context, 'cnext.repl', async () => {
        const terminal = vscode.window.createTerminal('Cnext REPL');
        terminal.sendText(`${getCompilerPath()} repl`);
        terminal.show();
    });

    registerCommand(context, 'cnext.restartLanguageServer', async () => {
        if (lsp) {
            await vscode.window.withProgress(
                { location: vscode.ProgressLocation.Notification, title: 'Restarting Cnext language server' },
                async () => lsp.start()
            );
            vscode.window.showInformationMessage(lsp.running ? 'Cnext language server restarted.' : 'Cnext language server failed to start. See output for details.');
        }
    });

    registerCommand(context, 'cnext.showLspLog', () => {
        outputChannel.show(true);
    });

    // --- Language server features ---
    if (lsp) {
        const docUri = (doc) => doc.uri.toString();

        context.subscriptions.push(
            vscode.languages.registerCompletionItemProvider('cnext', {
                provideCompletionItems: async (document, position) => {
                    if (!lsp.running) return [];
                    try {
                        return await lsp.completion(docUri(document), position);
                    } catch (err) {
                        return [];
                    }
                }
            }, '.', '(', '"'),

            vscode.languages.registerHoverProvider('cnext', {
                provideHover: async (document, position) => {
                    if (!lsp.running) return null;
                    try {
                        return await lsp.hover(docUri(document), position);
                    } catch (err) {
                        return null;
                    }
                }
            }),

            vscode.languages.registerDefinitionProvider('cnext', {
                provideDefinition: async (document, position) => {
                    if (!lsp.running) return null;
                    try {
                        return await lsp.definition(docUri(document), position);
                    } catch (err) {
                        return null;
                    }
                }
            }),

            vscode.languages.registerDocumentFormattingEditProvider('cnext', {
                provideDocumentFormattingEdits: async (document) => {
                    if (!lsp.running) return [];
                    try {
                        return await lsp.formatting(docUri(document), document.getText());
                    } catch (err) {
                        return [];
                    }
                }
            })
        );

        context.subscriptions.push(
            vscode.workspace.onDidOpenTextDocument((document) => {
                if (isCnextFile(document) && lsp.running) {
                    lsp.notify('textDocument/didOpen', lsp.documentParams(document));
                }
            }),
            vscode.workspace.onDidChangeTextDocument((event) => {
                if (isCnextFile(event.document) && lsp.running) {
                    lsp.notify('textDocument/didChange', {
                        textDocument: { uri: event.document.uri.toString(), version: 1 },
                        contentChanges: [{ text: event.document.getText() }]
                    });
                }
            }),
            vscode.workspace.onDidCloseTextDocument((document) => {
                if (isCnextFile(document) && lsp.running) {
                    lsp.notify('textDocument/didClose', lsp.documentId(document));
                }
            }),
            vscode.workspace.onDidSaveTextDocument(async (document) => {
                if (!isCnextFile(document)) return;
                const config = vscode.workspace.getConfiguration('cnext');
                if (config.get('autoFormat', false)) {
                    const edits = await vscode.commands.executeCommand('vscode.executeFormatDocumentProvider', document.uri);
                    if (edits && edits.length) {
                        const edit = new vscode.WorkspaceEdit();
                        edit.set(document.uri, edits);
                        await vscode.workspace.applyEdit(edit);
                    }
                }
                if (config.get('autoRun', false)) {
                    await runCommand('run', [document.fileName]);
                }
            })
        );
    }

    // --- Auto-run / auto-format on save (compiler fallback when LSP is off) ---
    context.subscriptions.push(vscode.workspace.onDidSaveTextDocument(async (document) => {
        if (!isCnextFile(document)) return;
        const config = vscode.workspace.getConfiguration('cnext');

        if (config.get('autoFormat', false) && !lsp.running) {
            await runCommand('fmt', [document.fileName]);
        }
        if (config.get('autoRun', false)) {
            await runCommand('run', [document.fileName]);
        }
    }));

    activateClient();
}

function deactivate() {
    if (lsp) lsp.dispose();
    if (statusBar) statusBar.dispose();
    if (outputChannel) outputChannel.dispose();
}

module.exports = { activate, deactivate };