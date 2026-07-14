let explorer, tabManager, terminalManager, diagnosticsManager, commandPalette;

async function init() {
  await window.editorAPI.initEditor();

  explorer = new FileExplorer();
  tabManager = new TabManager();
  terminalManager = new TerminalManager();
  diagnosticsManager = new DiagnosticsManager();
  commandPalette = new CommandPalette();

  window.explorer = explorer;
  window.tabManager = tabManager;
  window.terminalManager = terminalManager;
  window.diagnosticsManager = diagnosticsManager;
  window.commandPalette = commandPalette;

  setupPanelTabs();
  setupSidebarTabs();
  setupSidebarResize();
  setupPanelResize();
  setupMenuHandlers();
  setupKeyboardShortcuts();
  setupToolbar();

  initFind();
  initGit();
  await initSettings();
  applySettings();

  document.getElementById('open-folder-btn').addEventListener('click', () => explorer.openFolder());
  document.getElementById('welcome-open-btn').addEventListener('click', () => explorer.openFolder());
  document.getElementById('welcome-new-file-btn').addEventListener('click', () => {
    const content = '// Cnext program\nmain {\n\tprintln("Hello, World!")\n}';
    const blob = new Blob([content], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'main.cn';
    a.click();
    URL.revokeObjectURL(url);
  });

  const workspaceData = await window.api.getWorkspace();
  if (workspaceData && workspaceData.folder) {
    explorer.rootPath = workspaceData.folder;
    explorer.expandedDirs.add(workspaceData.folder);
    document.getElementById('project-name').textContent = workspaceData.folder.split(/[\\\/]/).pop();
    document.getElementById('welcome-screen').style.display = 'none';
    document.getElementById('breadcrumb-bar').style.display = 'flex';
    await explorer.refresh();
    if (window.updateGitBranch) window.updateGitBranch();
  }

  if (workspaceData && workspaceData.tabs && workspaceData.tabs.length > 0) {
    for (const tab of workspaceData.tabs) {
      await tabManager.openFile(tab.path, tab.name);
    }
    if (workspaceData.activeTabId) {
      const activeData = workspaceData.tabs.find(t => t.id === workspaceData.activeTabId);
      if (activeData) {
        const openedTab = tabManager.tabs.find(t => t.path === activeData.path);
        if (openedTab) tabManager.activateTab(openedTab.id);
      }
    }
  }

  await terminalManager.createTerminal();
}

window.saveWorkspaceState = function() {
  if (!window.api || !window.api.saveWorkspace) return;
  const data = {
    folder: explorer ? explorer.rootPath : null,
    tabs: tabManager ? tabManager.tabs.map(t => ({ id: t.id, path: t.path, name: t.name })) : [],
    activeTabId: tabManager && tabManager.activeTab ? tabManager.activeTab.id : null
  };
  window.api.saveWorkspace(data);
};

function setupToolbar() {
  document.getElementById('tb-open-folder').addEventListener('click', () => explorer.openFolder());
  document.getElementById('tb-save').addEventListener('click', () => tabManager.saveCurrent());
  document.getElementById('tb-save-all').addEventListener('click', () => {
    if (commandPalette) commandPalette.saveAll();
  });
  document.getElementById('tb-terminal').addEventListener('click', () => toggleBottomPanel());
  document.getElementById('tb-find').addEventListener('click', () => openFindPanel());
  document.getElementById('tb-settings').addEventListener('click', () => openSettings());
  document.getElementById('tb-command-palette').addEventListener('click', () => {
    if (commandPalette) commandPalette.open();
  });
  document.getElementById('tb-test').addEventListener('click', () => runTests());

  const runBtn = document.getElementById('run-btn');
  const runIcon = document.getElementById('run-icon');
  runBtn.addEventListener('click', async () => {
    runIcon.classList.add('spinning');
    runBtn.disabled = true;
    try {
      await diagnosticsManager.runCurrentFile();
    } finally {
      setTimeout(() => {
        runIcon.classList.remove('spinning');
        runBtn.disabled = false;
      }, 600);
    }
  });

  document.getElementById('build-btn').addEventListener('click', () => diagnosticsManager.buildCurrentFile());
  document.getElementById('new-terminal-btn').addEventListener('click', () => terminalManager.createTerminal());
  document.getElementById('clear-terminal-btn').addEventListener('click', () => terminalManager.clear());
  document.getElementById('toggle-panel-btn').addEventListener('click', () => toggleBottomPanel());
}

let panelCollapsed = false;
function toggleBottomPanel() {
  const panel = document.getElementById('bottom-panel');
  if (panelCollapsed) {
    panel.style.height = '';
    panel.style.display = 'flex';
    panelCollapsed = false;
    document.querySelectorAll('.panel-tab').forEach(t => t.classList.remove('active'));
    document.querySelectorAll('.bottom-content').forEach(c => c.classList.remove('active'));
    document.querySelector('.panel-tab[data-panel="terminal"]').classList.add('active');
    document.getElementById('terminal-panel').classList.add('active');
    setTimeout(() => terminalManager.resize(), 50);
  } else {
    panel.style.display = 'none';
    panelCollapsed = true;
  }
}

function setupPanelTabs() {
  document.querySelectorAll('.panel-tab[data-panel]').forEach(tab => {
    tab.addEventListener('click', () => {
      document.querySelectorAll('.panel-tab').forEach(t => t.classList.remove('active'));
      document.querySelectorAll('.bottom-content').forEach(c => c.classList.remove('active'));
      tab.classList.add('active');
      document.getElementById(`${tab.dataset.panel}-panel`).classList.add('active');
      if (tab.dataset.panel === 'terminal') {
        const panel = document.getElementById('bottom-panel');
        panel.style.display = 'flex';
        panelCollapsed = false;
        setTimeout(() => terminalManager.resize(), 50);
      }
    });
  });
}

function setupSidebarTabs() {
  document.querySelectorAll('.sidebar-tab').forEach(tab => {
    tab.addEventListener('click', () => {
      document.querySelectorAll('.sidebar-tab').forEach(t => t.classList.remove('active'));
      document.querySelectorAll('.sidebar-panel').forEach(p => p.classList.remove('active'));
      tab.classList.add('active');
      document.getElementById(`${tab.dataset.panel}-panel`).classList.add('active');

      if (tab.dataset.panel === 'git') {
        refreshGitStatus();
      }
    });
  });
}

function setupSidebarResize() {
  const sidebar = document.getElementById('sidebar');
  const handle = document.getElementById('sidebar-resize');
  let startX, startWidth;

  handle.addEventListener('mousedown', (e) => {
    startX = e.clientX;
    startWidth = sidebar.offsetWidth;
    document.addEventListener('mousemove', onDrag);
    document.addEventListener('mouseup', onUp);
    document.body.style.cursor = 'col-resize';
    document.body.style.userSelect = 'none';
  });

  function onDrag(e) {
    const diff = e.clientX - startX;
    sidebar.style.width = `${Math.max(160, Math.min(420, startWidth + diff))}px`;
  }

  function onUp() {
    document.removeEventListener('mousemove', onDrag);
    document.removeEventListener('mouseup', onUp);
    document.body.style.cursor = '';
    document.body.style.userSelect = '';
    terminalManager.resize();
  }
}

function setupPanelResize() {
  const panel = document.getElementById('bottom-panel');
  const handle = document.getElementById('panel-resize');
  let startY, startHeight;

  handle.addEventListener('mousedown', (e) => {
    startY = e.clientY;
    startHeight = panel.offsetHeight;
    document.addEventListener('mousemove', onDrag);
    document.addEventListener('mouseup', onUp);
    document.body.style.cursor = 'row-resize';
    document.body.style.userSelect = 'none';
  });

  function onDrag(e) {
    const diff = startY - e.clientY;
    const newH = Math.max(120, Math.min(600, startHeight + diff));
    panel.style.height = `${newH}px`;
  }

  function onUp() {
    document.removeEventListener('mousemove', onDrag);
    document.removeEventListener('mouseup', onUp);
    document.body.style.cursor = '';
    document.body.style.userSelect = '';
    terminalManager.resize();
  }
}

function setupMenuHandlers() {
  window.api.onMenuAction('menu-open-folder', () => explorer.openFolder());
  window.api.onMenuAction('menu-save', () => tabManager.saveCurrent());
  window.api.onMenuAction('menu-save-as', () => saveAs());
  window.api.onMenuAction('menu-build', () => diagnosticsManager.buildCurrentFile());
  window.api.onMenuAction('menu-run', () => diagnosticsManager.runCurrentFile());
  window.api.onMenuAction('menu-test', () => runTests());
  window.api.onMenuAction('menu-new-terminal', () => terminalManager.createTerminal());
  window.api.onMenuAction('menu-clear-terminal', () => terminalManager.clear());
}

function setupKeyboardShortcuts() {
  document.addEventListener('keydown', (e) => {
    if (e.ctrlKey && e.key === 's') { e.preventDefault(); tabManager.saveCurrent(); }
    if (e.ctrlKey && e.key === 'o') { e.preventDefault(); explorer.openFolder(); }
    if (e.key === 'F5' && !e.ctrlKey) { e.preventDefault(); diagnosticsManager.buildCurrentFile(); }
    if (e.ctrlKey && e.key === 'F5') { e.preventDefault(); diagnosticsManager.runCurrentFile(); }
    if (e.key === 'F5' && e.shiftKey) { e.preventDefault(); runTests(); }
    if (e.ctrlKey && e.key === '`') { e.preventDefault(); toggleBottomPanel(); }
    if (e.ctrlKey && e.shiftKey && e.key === 'F') {
      e.preventDefault();
      openFindPanel();
    }
    if (e.ctrlKey && e.key === ',') {
      e.preventDefault();
      openSettings();
    }
  });
}

function openFindPanel() {
  document.querySelectorAll('.sidebar-tab').forEach(t => t.classList.remove('active'));
  document.querySelectorAll('.sidebar-panel').forEach(p => p.classList.remove('active'));
  document.querySelector('[data-panel="search"]').classList.add('active');
  document.getElementById('find-panel').classList.add('active');
  setTimeout(() => {
    const input = document.getElementById('search-input');
    if (input) { input.focus(); input.select(); }
  }, 100);
}
window.openFindPanel = openFindPanel;

async function saveAs() {
  const content = window.editorAPI.getEditorContent();
  const activeFile = tabManager.getActiveFile();
  await window.api.saveFile(content, activeFile);
}

async function runTests() {
  const folder = explorer.rootPath;
  if (!folder) return;
  diagnosticsManager.showNotification('Running tests...');
  const result = await window.api.runCompiler(['test', folder]);
  diagnosticsManager.parseOutput(result);
}
window.runTests = runTests;

window.updateGitBranch = async function() {
  const rootPath = window.explorer ? window.explorer.rootPath : null;
  const branchEl = document.getElementById('git-branch-display');
  if (!rootPath || !branchEl) return;
  try {
    const branches = await window.api.gitBranch(rootPath);
    const current = branches.find(b => b.current);
    if (current) {
      branchEl.innerHTML = `<svg width="12" height="12" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.5"><circle cx="4" cy="4" r="1.5"/><circle cx="4" cy="12" r="1.5"/><circle cx="12" cy="12" r="1.5"/><line x1="4" y1="5.5" x2="4" y2="10.5"/><line x1="4.5" y1="10.5" x2="10.5" y2="10.5"/></svg> ${current.name}`;
    }
  } catch (e) {}
};

init();
