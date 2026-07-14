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

    function getOutputExe(filePath) {
        return process.platform === 'win32'
            ? filePath.replace(/\.cn$/, '.exe')
            : filePath.replace(/\.cn$/, '');
    }

    async function runCommand(command, args, cwd) {
        const compiler = getCompilerPath();
        const cmd = `${compiler} ${command} "${args}"`;
        outputChannel.appendLine(`> ${cmd}`);

        try {
            const { stdout, stderr } = await execPromise(cmd, { cwd, timeout: 30000 });
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

    let runCommand_handler = vscode.commands.registerCommand('cnext.run', async () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor) {
            vscode.window.showErrorMessage('No active editor');
            return;
        }

        const document = editor.document;
        if (document.languageId !== 'cnext') {
            vscode.window.showErrorMessage('Not a Cnext file');
            return;
        }

        await document.save();
        const filePath = document.fileName;

        const result = await runCommand('run', filePath, path.dirname(filePath));
        if (result.success && result.output.trim()) {
            vscode.window.showInformationMessage(`Output: ${result.output.trim()}`);
        }
    });

    let buildCommand_handler = vscode.commands.registerCommand('cnext.build', async () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor) {
            vscode.window.showErrorMessage('No active editor');
            return;
        }

        const document = editor.document;
        if (document.languageId !== 'cnext') {
            vscode.window.showErrorMessage('Not a Cnext file');
            return;
        }

        await document.save();
        const filePath = document.fileName;
        const outputFile = getOutputExe(filePath);

        const result = await runCommand(`build "${filePath}" -o "${outputFile}"`, '', path.dirname(filePath));
        if (result.success) {
            vscode.window.showInformationMessage(`Built: ${outputFile}`);
        }
    });

    let testCommand_handler = vscode.commands.registerCommand('cnext.test', async () => {
        const workspaceFolder = vscode.workspace.workspaceFolders?.[0];
        if (!workspaceFolder) {
            vscode.window.showErrorMessage('No workspace folder');
            return;
        }

        await runCommand('test', '', workspaceFolder.uri.fsPath);
    });

    let onSave = vscode.workspace.onDidSaveTextDocument(async (document) => {
        if (document.languageId !== 'cnext') return;

        const config = vscode.workspace.getConfiguration('cnext');
        if (!config.get('autoRun', false)) return;

        const filePath = document.fileName;
        await runCommand('run', filePath, path.dirname(filePath));
    });

    context.subscriptions.push(runCommand_handler, buildCommand_handler, testCommand_handler, onSave, outputChannel);
}

function deactivate() {
    if (outputChannel) {
        outputChannel.dispose();
    }
}

module.exports = { activate, deactivate };
