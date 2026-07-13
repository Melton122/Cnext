const vscode = require('vscode');
const { execSync } = require('child_process');
const path = require('path');

function activate(context) {
    console.log('Cnext extension activated');

    // Get compiler path from settings
    function getCompilerPath() {
        const config = vscode.workspace.getConfiguration('cnext');
        return config.get('compilerPath', 'cnext');
    }

    // Run Cnext file
    let runCommand = vscode.commands.registerCommand('cnext.run', async () => {
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

        // Save the file
        await document.save();

        const filePath = document.fileName;
        const compiler = getCompilerPath();

        try {
            const output = execSync(`${compiler} run "${filePath}"`, {
                encoding: 'utf-8',
                cwd: path.dirname(filePath)
            });
            vscode.window.showInformationMessage(`Output: ${output.trim()}`);
        } catch (error) {
            vscode.window.showErrorMessage(`Error: ${error.message}`);
        }
    });

    // Build Cnext file
    let buildCommand = vscode.commands.registerCommand('cnext.build', async () => {
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
        const compiler = getCompilerPath();
        const outputFile = filePath.replace(/\.cn$/, '.exe');

        try {
            execSync(`${compiler} build "${filePath}" -o "${outputFile}"`, {
                encoding: 'utf-8',
                cwd: path.dirname(filePath)
            });
            vscode.window.showInformationMessage(`Built: ${outputFile}`);
        } catch (error) {
            vscode.window.showErrorMessage(`Build failed: ${error.message}`);
        }
    });

    // Run tests
    let testCommand = vscode.commands.registerCommand('cnext.test', async () => {
        const compiler = getCompilerPath();
        const workspaceFolder = vscode.workspace.workspaceFolders?.[0];

        if (!workspaceFolder) {
            vscode.window.showErrorMessage('No workspace folder');
            return;
        }

        try {
            const output = execSync(`${compiler} test`, {
                encoding: 'utf-8',
                cwd: workspaceFolder.uri.fsPath
            });
            vscode.window.showInformationMessage(output.trim());
        } catch (error) {
            vscode.window.showErrorMessage(`Tests failed: ${error.message}`);
        }
    });

    // Auto-run on save
    let onSave = vscode.workspace.onDidSaveTextDocument(async (document) => {
        if (document.languageId !== 'cnext') return;

        const config = vscode.workspace.getConfiguration('cnext');
        if (!config.get('autoRun', false)) return;

        const compiler = getCompilerPath();
        const filePath = document.fileName;

        try {
            const output = execSync(`${compiler} run "${filePath}"`, {
                encoding: 'utf-8',
                cwd: path.dirname(filePath)
            });
            // Output will be shown in terminal
        } catch (error) {
            // Silently fail on auto-run
        }
    });

    context.subscriptions.push(runCommand, buildCommand, testCommand, onSave);
}

function deactivate() {}

module.exports = { activate, deactivate };
