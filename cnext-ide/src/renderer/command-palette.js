class CommandPalette {
  constructor() {
    this.commands = [
      { label: 'Open Folder', description: 'Open a folder in the workspace', action: () => window.explorer && window.explorer.openFolder(), keybinding: 'Ctrl+O' },
      { label: 'Save', description: 'Save the current file', action: () => window.tabManager && window.tabManager.saveCurrent(), keybinding: 'Ctrl+S' },
      { label: 'Save All', description: 'Save all open files', action: () => this.saveAll(), keybinding: 'Ctrl+Shift+S' },
      { label: 'Build', description: 'Build the current file', action: () => window.diagnosticsManager && window.diagnosticsManager.buildCurrentFile(), keybinding: 'F5' },
      { label: 'Run', description: 'Run the current file', action: () => window.diagnosticsManager && window.diagnosticsManager.runCurrentFile(), keybinding: 'Ctrl+F5' },
      { label: 'Run Tests', description: 'Run all tests', action: () => window.runTests && window.runTests(), keybinding: 'Shift+F5' },
      { label: 'New Terminal', description: 'Open a new terminal', action: () => window.terminalManager && window.terminalManager.createTerminal(), keybinding: 'Ctrl+`' },
      { label: 'Clear Terminal', description: 'Clear the active terminal', action: () => window.terminalManager && window.terminalManager.clear() },
      { label: 'Find in Files', description: 'Search across all files', action: () => window.openFindPanel && window.openFindPanel(), keybinding: 'Ctrl+Shift+F' },
      { label: 'Open Settings', description: 'Open IDE settings', action: () => window.openSettings && window.openSettings(), keybinding: 'Ctrl+,' },
      { label: 'Toggle Word Wrap', description: 'Toggle word wrapping', action: () => this.toggleWordWrap() },
      { label: 'Increase Font Size', description: 'Make text larger', action: () => this.changeFontSize(1) },
      { label: 'Decrease Font Size', description: 'Make text smaller', action: () => this.changeFontSize(-1) },
      { label: 'Reset Font Size', description: 'Reset to default size', action: () => this.resetFontSize() },
      { label: 'Toggle Minimap', description: 'Show/hide minimap', action: () => this.toggleMinimap() },
      { label: 'Format Document', description: 'Format the current document', action: () => this.formatDocument() },
      { label: 'Close Tab', description: 'Close the active tab', action: () => this.closeActiveTab() },
      { label: 'Close All Tabs', description: 'Close all open tabs', action: () => this.closeAllTabs() },
    ];

    this.modal = document.getElementById('command-palette');
    this.input = document.getElementById('command-input');
    this.list = document.getElementById('command-list');
    this.isOpen = false;

    this.init();
  }

  init() {
    if (this.input) {
      this.input.addEventListener('input', () => this.filterCommands(this.input.value));
      this.input.addEventListener('keydown', (e) => {
        if (e.key === 'Escape') this.close();
        if (e.key === 'Enter') this.executeSelected();
        if (e.key === 'ArrowDown') this.selectNext();
        if (e.key === 'ArrowUp') this.selectPrev();
      });
    }

    if (this.modal) {
      this.modal.addEventListener('click', (e) => {
        if (e.target === this.modal) this.close();
      });
    }

    document.addEventListener('keydown', (e) => {
      if (e.ctrlKey && e.shiftKey && e.key === 'P') {
        e.preventDefault();
        this.toggle();
      }
    });
  }

  toggle() {
    if (this.isOpen) this.close();
    else this.open();
  }

  open() {
    if (!this.modal) return;
    this.modal.classList.add('active');
    this.isOpen = true;
    this.input.value = '';
    this.filterCommands('');
    setTimeout(() => this.input.focus(), 50);
  }

  close() {
    if (!this.modal) return;
    this.modal.classList.remove('active');
    this.isOpen = false;
  }

  filterCommands(query) {
    const q = query.toLowerCase().trim();
    const filtered = q
      ? this.commands.filter(c => c.label.toLowerCase().includes(q) || c.description.toLowerCase().includes(q))
      : this.commands;

    this.list.innerHTML = '';
    filtered.forEach((cmd, i) => {
      const el = document.createElement('div');
      el.className = `command-item${i === 0 ? ' selected' : ''}`;
      el.innerHTML = `
        <div class="command-label">${cmd.label}</div>
        <div class="command-desc">${cmd.description}</div>
        ${cmd.keybinding ? `<div class="command-key"><kbd>${cmd.keybinding}</kbd></div>` : ''}
      `;
      el.addEventListener('click', () => { cmd.action(); this.close(); });
      el.addEventListener('mouseenter', () => {
        this.list.querySelectorAll('.command-item').forEach(e => e.classList.remove('selected'));
        el.classList.add('selected');
      });
      this.list.appendChild(el);
    });
  }

  executeSelected() {
    const selected = this.list.querySelector('.command-item.selected');
    if (selected) selected.click();
  }

  selectNext() {
    const items = this.list.querySelectorAll('.command-item');
    let idx = -1;
    items.forEach((el, i) => { if (el.classList.contains('selected')) idx = i; });
    items.forEach(el => el.classList.remove('selected'));
    const next = Math.min(idx + 1, items.length - 1);
    if (items[next]) items[next].classList.add('selected');
  }

  selectPrev() {
    const items = this.list.querySelectorAll('.command-item');
    let idx = 0;
    items.forEach((el, i) => { if (el.classList.contains('selected')) idx = i; });
    items.forEach(el => el.classList.remove('selected'));
    const prev = Math.max(idx - 1, 0);
    if (items[prev]) items[prev].classList.add('selected');
  }

  changeFontSize(delta) {
    const ed = window.editorAPI && window.editorAPI.getEditor();
    if (ed) {
      const current = ed.getOption(14);
      ed.updateOptions({ fontSize: current + delta });
    }
  }

  resetFontSize() {
    const ed = window.editorAPI && window.editorAPI.getEditor();
    if (ed) ed.updateOptions({ fontSize: 14 });
  }

  toggleWordWrap() {
    const ed = window.editorAPI && window.editorAPI.getEditor();
    if (ed) {
      const current = ed.getOption(107);
      ed.updateOptions({ wordWrap: current === 'off' ? 'on' : 'off' });
    }
  }

  toggleMinimap() {
    const ed = window.editorAPI && window.editorAPI.getEditor();
    if (ed) {
      const current = ed.getOption(62);
      ed.updateOptions({ minimap: { enabled: !current.enabled } });
    }
  }

  formatDocument() {
    const ed = window.editorAPI && window.editorAPI.getEditor();
    if (ed) {
      ed.getAction('editor.action.formatDocument')?.run();
    }
  }

  closeActiveTab() {
    if (window.tabManager && window.tabManager.activeTab) {
      window.tabManager.closeTab(window.tabManager.activeTab.id);
    }
  }

  closeAllTabs() {
    if (window.tabManager) {
      const ids = window.tabManager.tabs.map(t => t.id);
      ids.forEach(id => window.tabManager.closeTab(id));
    }
  }

  async saveAll() {
    if (window.tabManager) {
      for (const tab of window.tabManager.tabs) {
        if (tab.modified) {
          const content = window.editorAPI.getEditorContent();
          await window.api.writeFile(tab.path, content);
          tab.originalContent = content;
          tab.modified = false;
        }
      }
      window.tabManager.renderTabs();
    }
  }
}

window.CommandPalette = CommandPalette;
