const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('api', {
  // File operations
  readFile: (filePath) => ipcRenderer.invoke('read-file', filePath),
  writeFile: (filePath, content) => ipcRenderer.invoke('write-file', filePath, content),
  readDir: (dirPath) => ipcRenderer.invoke('read-dir', dirPath),
  createFile: (filePath) => ipcRenderer.invoke('create-file', filePath),
  createDir: (dirPath) => ipcRenderer.invoke('create-dir', dirPath),
  deletePath: (filePath) => ipcRenderer.invoke('delete-path', filePath),
  renamePath: (oldPath, newPath) => ipcRenderer.invoke('rename-path', oldPath, newPath),
  stat: (filePath) => ipcRenderer.invoke('stat', filePath),

  // Dialog
  openFolder: () => ipcRenderer.invoke('open-folder'),
  saveFile: (content, defaultPath) => ipcRenderer.invoke('save-file', content, defaultPath),

  // Compiler
  runCompiler: (args) => ipcRenderer.invoke('run-compiler', args),

  // Terminal
  createTerminal: (cwd) => ipcRenderer.invoke('create-terminal', cwd),
  terminalWrite: (id, data) => ipcRenderer.invoke('terminal-write', id, data),
  terminalResize: (id, cols, rows) => ipcRenderer.invoke('terminal-resize', id, cols, rows),
  onTerminalData: (callback) => ipcRenderer.on('terminal-data', (_, id, data) => callback(id, data)),
  onTerminalExit: (callback) => ipcRenderer.on('terminal-exit', (_, id) => callback(id)),

  // Search
  searchInFiles: (dirPath, query, caseSensitive) => ipcRenderer.invoke('search-in-files', dirPath, query, caseSensitive),

  // Git
  gitStatus: (repoPath) => ipcRenderer.invoke('git-status', repoPath),
  gitDiff: (repoPath, filePath) => ipcRenderer.invoke('git-diff', repoPath, filePath),
  gitCommit: (repoPath, message) => ipcRenderer.invoke('git-commit', repoPath, message),
  gitLog: (repoPath) => ipcRenderer.invoke('git-log', repoPath),
  gitBranch: (repoPath) => ipcRenderer.invoke('git-branch', repoPath),
  gitAdd: (repoPath, filePath) => ipcRenderer.invoke('git-add', repoPath, filePath),

  // Settings
  getSettings: () => ipcRenderer.invoke('get-settings'),
  saveSettings: (settings) => ipcRenderer.invoke('save-settings', settings),

  // Workspace
  getWorkspace: () => ipcRenderer.invoke('get-workspace'),
  saveWorkspace: (data) => ipcRenderer.invoke('save-workspace', data),

  // Menu events
  onMenuAction: (channel, callback) => {
    ipcRenderer.on(channel, (_, ...args) => callback(...args));
  },

  // App
  getPlatform: () => process.platform,
  getCnextPath: () => ipcRenderer.invoke('get-cnext-path'),
  readWorkerScript: () => ipcRenderer.invoke('read-worker-script')
});
