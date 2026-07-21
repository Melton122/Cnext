const vscode = require('vscode');
const { exec } = require('child_process');
const path = require('path');
const util = require('util');
const execPromise = util.promisify(exec);

let outputChannel;

function activate(context) {
    outputChannel = vscode.window.createOutputChannel('Cnext');
    outputChannel.appendLine('Cnext extension activated');

    function getCompilerPath() {
        const config = vscode.workspace.getConfiguration('cnext');
        return config.get('compilerPath', 'cnext');
    }

    function getCwd() {
        const workspaceFolder = vscode.workspace.workspaceFolders?.[0];
        return workspaceFolder ? workspaceFolder.uri.fsPath : undefined;
    }

    async function runCommand(command, args, cwd) {
        const compiler = getCompilerPath();
        const fullArgs = args ? ` ${args}` : '';
        const cmd = `${compiler} ${command}${fullArgs}`;
        outputChannel.appendLine(`> ${cmd}`);

        try {
            const { stdout, stderr } = await execPromise(cmd, { cwd: cwd || getCwd(), timeout: 30000 });
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

    // Run file
    context.subscriptions.push(vscode.commands.registerCommand('cnext.run', async () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor || !isCnextFile(editor.document)) {
            vscode.window.showErrorMessage('Open a .cn file first');
            return;
        }
        await editor.document.save();
        const result = await runCommand('run', `"${editor.document.fileName}"`);
        if (result.success && result.output.trim()) {
            vscode.window.showInformationMessage(`Output: ${result.output.trim().split('\n')[0]}`);
        }
    }));

    // Build file
    context.subscriptions.push(vscode.commands.registerCommand('cnext.build', async () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor || !isCnextFile(editor.document)) {
            vscode.window.showErrorMessage('Open a .cn file first');
            return;
        }
        await editor.document.save();
        const ext = process.platform === 'win32' ? '.exe' : '';
        const outputFile = editor.document.fileName.replace(/\.cn$/, ext);
        const result = await runCommand('build', `"${editor.document.fileName}" -o "${outputFile}"`);
        if (result.success) {
            vscode.window.showInformationMessage(`Built: ${path.basename(outputFile)}`);
        }
    }));

    // Build release
    context.subscriptions.push(vscode.commands.registerCommand('cnext.buildRelease', async () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor || !isCnextFile(editor.document)) {
            vscode.window.showErrorMessage('Open a .cn file first');
            return;
        }
        await editor.document.save();
        const ext = process.platform === 'win32' ? '.exe' : '';
        const outputFile = editor.document.fileName.replace(/\.cn$/, ext);
        const result = await runCommand('build', `"${editor.document.fileName}" -o "${outputFile}" --release`);
        if (result.success) {
            vscode.window.showInformationMessage(`Release built: ${path.basename(outputFile)}`);
        }
    }));

    // Run tests
    context.subscriptions.push(vscode.commands.registerCommand('cnext.test', async () => {
        await runCommand('test', '');
    }));

    // Format file
    context.subscriptions.push(vscode.commands.registerCommand('cnext.format', async () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor || !isCnextFile(editor.document)) {
            vscode.window.showErrorMessage('Open a .cn file first');
            return;
        }
        await editor.document.save();
        const result = await runCommand('fmt', `"${editor.document.fileName}"`);
        if (result.success) {
            vscode.window.showInformationMessage('Formatted');
            await vscode.commands.executeCommand('workbench.action.revertAndCloseActiveEditor');
            await vscode.window.showTextDocument(vscode.Uri.file(editor.document.fileName));
        }
    }));

    // Lint file
    context.subscriptions.push(vscode.commands.registerCommand('cnext.lint', async () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor || !isCnextFile(editor.document)) {
            vscode.window.showErrorMessage('Open a .cn file first');
            return;
        }
        await runCommand('lint', `"${editor.document.fileName}"`);
    }));

    // New project
    context.subscriptions.push(vscode.commands.registerCommand('cnext.newProject', async () => {
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
        await runCommand('new', name);
        vscode.window.showInformationMessage(`Project "${name}" created. Open the folder to start coding!`);
    }));

    // Doctor
    context.subscriptions.push(vscode.commands.registerCommand('cnext.doctor', async () => {
        await runCommand('doctor', '');
    }));

    // REPL
    context.subscriptions.push(vscode.commands.registerCommand('cnext.repl', async () => {
        const terminal = vscode.window.createTerminal('Cnext REPL');
        terminal.sendText(`${getCompilerPath()} repl`);
        terminal.show();
    }));

    // Auto-run / auto-format on save
    context.subscriptions.push(vscode.workspace.onDidSaveTextDocument(async (document) => {
        if (!isCnextFile(document)) return;
        const config = vscode.workspace.getConfiguration('cnext');

        if (config.get('autoFormat', false)) {
            await runCommand('fmt', `"${document.fileName}"`);
        }
        if (config.get('autoRun', false)) {
            await runCommand('run', `"${document.fileName}"`);
        }
    }));
}

function deactivate() {
    if (outputChannel) outputChannel.dispose();
}

module.exports = { activate, deactivate };
