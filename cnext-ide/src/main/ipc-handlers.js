const { ipcMain, dialog, BrowserWindow } = require('electron');
const fs = require('fs');
const path = require('path');

let pty;
try {
  pty = require('node-pty');
} catch (e) {
  console.warn('node-pty not available, terminal will use fallback');
}

let terminalId = 0;
const terminals = {};

function setupIPC(mainWindow) {
  // File operations
  ipcMain.handle('read-file', async (_, filePath) => {
    try {
      return fs.readFileSync(filePath, 'utf-8');
    } catch (e) {
      return null;
    }
  });

  ipcMain.handle('write-file', async (_, filePath, content) => {
    try {
      fs.writeFileSync(filePath, content, 'utf-8');
      return true;
    } catch (e) {
      return false;
    }
  });

  ipcMain.handle('read-dir', async (_, dirPath) => {
    try {
      const entries = fs.readdirSync(dirPath, { withFileTypes: true });
      return entries.map(entry => ({
        name: entry.name,
        path: path.join(dirPath, entry.name),
        isDirectory: entry.isDirectory(),
        isFile: entry.isFile()
      }));
    } catch (e) {
      return [];
    }
  });

  ipcMain.handle('create-file', async (_, filePath) => {
    try {
      fs.writeFileSync(filePath, '', 'utf-8');
      return true;
    } catch (e) {
      return false;
    }
  });

  ipcMain.handle('create-dir', async (_, dirPath) => {
    try {
      fs.mkdirSync(dirPath, { recursive: true });
      return true;
    } catch (e) {
      return false;
    }
  });

  ipcMain.handle('delete-path', async (_, filePath) => {
    try {
      const stat = fs.statSync(filePath);
      if (stat.isDirectory()) {
        fs.rmSync(filePath, { recursive: true });
      } else {
        fs.unlinkSync(filePath);
      }
      return true;
    } catch (e) {
      return false;
    }
  });

  ipcMain.handle('rename-path', async (_, oldPath, newPath) => {
    try {
      fs.renameSync(oldPath, newPath);
      return true;
    } catch (e) {
      return false;
    }
  });

  ipcMain.handle('stat', async (_, filePath) => {
    try {
      const stat = fs.statSync(filePath);
      return { isFile: stat.isFile(), isDirectory: stat.isDirectory(), size: stat.size };
    } catch (e) {
      return null;
    }
  });

  // Dialogs
  ipcMain.handle('open-folder', async () => {
    const result = await dialog.showOpenDialog(mainWindow, {
      properties: ['openDirectory']
    });
    if (result.canceled) return null;
    return result.filePaths[0];
  });

  ipcMain.handle('save-file', async (_, content, defaultPath) => {
    const result = await dialog.showSaveDialog(mainWindow, {
      defaultPath: defaultPath,
      filters: [{ name: 'Cnext Files', extensions: ['cn'] }, { name: 'All Files', extensions: ['*'] }]
    });
    if (result.canceled) return null;
    fs.writeFileSync(result.filePath, content, 'utf-8');
    return result.filePath;
  });

  // Terminal (node-pty)
  ipcMain.handle('create-terminal', async (_, cwd) => {
    const id = terminalId++;
    const workDir = cwd || process.cwd();

    if (pty) {
      const shell = process.platform === 'win32' ? 'cmd.exe' : 'bash';
      const term = pty.spawn(shell, [], {
        name: 'xterm-256color',
        cols: 80,
        rows: 24,
        cwd: workDir,
        env: { ...process.env, TERM: 'xterm-256color' }
      });

      terminals[id] = { ptyProc: term, id };

      term.onData((data) => {
        mainWindow.webContents.send('terminal-data', id, data);
      });

      term.onExit(() => {
        mainWindow.webContents.send('terminal-exit', id);
        delete terminals[id];
      });
    } else {
      const { spawn } = require('child_process');
      const shell = process.platform === 'win32' ? 'cmd.exe' : 'bash';
      const proc = spawn(shell, [], { cwd: workDir, env: process.env });

      terminals[id] = { proc, id };

      proc.stdout.on('data', (data) => {
        mainWindow.webContents.send('terminal-data', id, data.toString());
      });
      proc.stderr.on('data', (data) => {
        mainWindow.webContents.send('terminal-data', id, data.toString());
      });
      proc.on('close', () => {
        mainWindow.webContents.send('terminal-exit', id);
        delete terminals[id];
      });

      proc.stdin.write = proc.stdin.write.bind(proc.stdin);
    }

    return id;
  });

  ipcMain.handle('terminal-write', async (_, id, data) => {
    const t = terminals[id];
    if (!t) return;
    if (t.ptyProc) {
      t.ptyProc.write(data);
    } else if (t.proc) {
      t.proc.stdin.write(data);
    }
  });

  ipcMain.handle('terminal-resize', async (_, id, cols, rows) => {
    const t = terminals[id];
    if (!t) return;
    if (t.ptyProc) {
      t.ptyProc.resize(cols, rows);
    }
  });

  // Compiler
  ipcMain.handle('run-compiler', async (_, args) => {
    return new Promise((resolve) => {
      const { spawn } = require('child_process');
      const exe = process.platform === 'win32' ? 'cnext.exe' : 'cnext';
      const proc = spawn(exe, args, { shell: true });
      let stdout = '';
      let stderr = '';

      proc.stdout.on('data', (data) => { stdout += data; });
      proc.stderr.on('data', (data) => { stderr += data; });

      proc.on('close', (code) => {
        resolve({ code, stdout, stderr });
      });

      proc.on('error', () => {
        resolve({ code: 127, stdout: '', stderr: 'Could not find cnext compiler' });
      });
    });
  });

  ipcMain.handle('get-cnext-path', () => {
    const exe = process.platform === 'win32' ? 'cnext.exe' : 'cnext';
    return exe;
  });

  ipcMain.handle('read-worker-script', () => {
    try {
      const workerPath = path.join(__dirname, '..', 'renderer', 'lib', 'vs', 'base', 'worker', 'workerMain.js');
      return fs.readFileSync(workerPath, 'utf-8');
    } catch (e) {
      return '';
    }
  });

  // Find in files
  ipcMain.handle('search-in-files', async (_, dirPath, query, caseSensitive) => {
    const results = [];
    const regex = new RegExp(query, caseSensitive ? 'g' : 'gi');

    function searchDir(dir) {
      try {
        const entries = fs.readdirSync(dir, { withFileTypes: true });
        for (const entry of entries) {
          if (entry.name.startsWith('.') || entry.name === 'node_modules') continue;
          const fullPath = path.join(dir, entry.name);
          if (entry.isDirectory()) {
            searchDir(fullPath);
          } else if (entry.isFile()) {
            const ext = path.extname(entry.name).toLowerCase();
            if (!['.cn', '.c', '.h', '.js', '.json', '.md', '.toml', '.txt', '.py', '.rs'].includes(ext)) continue;
            try {
              const content = fs.readFileSync(fullPath, 'utf-8');
              const lines = content.split('\n');
              for (let i = 0; i < lines.length; i++) {
                if (regex.test(lines[i])) {
                  results.push({ file: fullPath, line: i + 1, text: lines[i].trim() });
                  regex.lastIndex = 0;
                }
              }
            } catch (e) {}
          }
        }
      } catch (e) {}
    }

    searchDir(dirPath);
    return results;
  });

  // Git operations
  ipcMain.handle('git-status', async (_, repoPath) => {
    return new Promise((resolve) => {
      const { spawn } = require('child_process');
      const proc = spawn('git', ['status', '--porcelain'], { cwd: repoPath, shell: true });
      let stdout = '';
      proc.stdout.on('data', (d) => { stdout += d; });
      proc.on('close', () => {
        const files = stdout.trim().split('\n').filter(l => l).map(line => ({
          status: line.substring(0, 2).trim(),
          file: line.substring(3).trim()
        }));
        resolve(files);
      });
      proc.on('error', () => resolve([]));
    });
  });

  ipcMain.handle('git-diff', async (_, repoPath, filePath) => {
    return new Promise((resolve) => {
      const { spawn } = require('child_process');
      const args = ['diff'];
      if (filePath) args.push(filePath);
      const proc = spawn('git', args, { cwd: repoPath, shell: true });
      let stdout = '';
      proc.stdout.on('data', (d) => { stdout += d; });
      proc.on('close', () => resolve(stdout));
      proc.on('error', () => resolve(''));
    });
  });

  ipcMain.handle('git-commit', async (_, repoPath, message) => {
    return new Promise((resolve) => {
      const { spawn } = require('child_process');
      const proc = spawn('git', ['commit', '-m', message], { cwd: repoPath, shell: true });
      let stdout = '';
      let stderr = '';
      proc.stdout.on('data', (d) => { stdout += d; });
      proc.stderr.on('data', (d) => { stderr += d; });
      proc.on('close', (code) => resolve({ code, stdout, stderr }));
      proc.on('error', () => resolve({ code: 1, stdout: '', stderr: 'git not found' }));
    });
  });

  ipcMain.handle('git-log', async (_, repoPath) => {
    return new Promise((resolve) => {
      const { spawn } = require('child_process');
      const proc = spawn('git', ['log', '--oneline', '-20'], { cwd: repoPath, shell: true });
      let stdout = '';
      proc.stdout.on('data', (d) => { stdout += d; });
      proc.on('close', () => {
        const entries = stdout.trim().split('\n').filter(l => l).map(line => {
          const spaceIdx = line.indexOf(' ');
          return { hash: line.substring(0, spaceIdx), message: line.substring(spaceIdx + 1) };
        });
        resolve(entries);
      });
      proc.on('error', () => resolve([]));
    });
  });

  ipcMain.handle('git-branch', async (_, repoPath) => {
    return new Promise((resolve) => {
      const { spawn } = require('child_process');
      const proc = spawn('git', ['branch', '--no-color'], { cwd: repoPath, shell: true });
      let stdout = '';
      proc.stdout.on('data', (d) => { stdout += d; });
      proc.on('close', () => {
        const branches = stdout.trim().split('\n').filter(l => l).map(l => ({
          name: l.replace(/^\*?\s+/, ''),
          current: l.startsWith('*')
        }));
        resolve(branches);
      });
      proc.on('error', () => resolve([]));
    });
  });

  ipcMain.handle('git-add', async (_, repoPath, filePath) => {
    return new Promise((resolve) => {
      const { spawn } = require('child_process');
      const proc = spawn('git', ['add', filePath], { cwd: repoPath, shell: true });
      proc.on('close', (code) => resolve(code === 0));
      proc.on('error', () => resolve(false));
    });
  });

  // Settings
  const settingsPath = path.join(require('os').homedir(), '.cnext-ide', 'settings.json');

  ipcMain.handle('get-settings', async () => {
    try {
      return JSON.parse(fs.readFileSync(settingsPath, 'utf-8'));
    } catch (e) {
      return { compilerPath: 'cnext', fontSize: 14, tabSize: 4, fontFamily: "'Cascadia Code', 'Fira Code', monospace", autoSave: false, terminalShell: '' };
    }
  });

  ipcMain.handle('save-settings', async (_, settings) => {
    try {
      const dir = path.dirname(settingsPath);
      if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });
      fs.writeFileSync(settingsPath, JSON.stringify(settings, null, 2), 'utf-8');
      return true;
    } catch (e) {
      return false;
    }
  });

  const workspacePath = path.join(require('os').homedir(), '.cnext-ide', 'workspace.json');

  ipcMain.handle('get-workspace', async () => {
    try {
      return JSON.parse(fs.readFileSync(workspacePath, 'utf-8'));
    } catch (e) {
      return { folder: null, tabs: [], activeTabId: null };
    }
  });

  ipcMain.handle('save-workspace', async (_, data) => {
    try {
      const dir = path.dirname(workspacePath);
      if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });
      fs.writeFileSync(workspacePath, JSON.stringify(data, null, 2), 'utf-8');
      return true;
    } catch (e) {
      return false;
    }
  });
}

module.exports = { setupIPC };
