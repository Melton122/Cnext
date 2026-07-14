class TabManager {
  constructor() {
    this.tabs = [];
    this.activeTab = null;
    this.tabsEl = document.getElementById('tabs');
  }

  getFileIcon(name) {
    const ext = name.split('.').pop().toLowerCase();
    const colors = {
      cn: '#7c5ce0', c: '#5586c2', h: '#5586c2', js: '#d4a72c',
      json: '#6b6b6b', md: '#519aba', py: '#3572A5', toml: '#9c6b20',
      txt: '#6b6b6b', rs: '#dea584', yaml: '#cb171e', css: '#563d7c',
    };
    const color = colors[ext] || '#6b6b6b';
    const label = ext === 'json' ? '{ }' : ext.length > 2 ? ext.substring(0, 2) : ext.toUpperCase();
    return `<svg width="12" height="12" viewBox="0 0 16 16"><rect x="1" y="1" width="14" height="14" rx="3" fill="${color}"/><text x="8" y="11.5" text-anchor="middle" font-size="${label.length > 1 ? 6 : 8}" font-weight="bold" fill="white" font-family="monospace">${label}</text></svg>`;
  }

  async openFile(filePath, fileName) {
    const existing = this.tabs.find(t => t.path === filePath);
    if (existing) {
      this.activateTab(existing.id);
      return;
    }

    const content = await window.api.readFile(filePath);
    if (content === null) return;

    const tab = {
      id: Date.now().toString(),
      path: filePath,
      name: fileName || filePath.split(/[\\/]/).pop(),
      content: content,
      originalContent: content,
      modified: false
    };

    this.tabs.push(tab);
    this.renderTabs();
    this.activateTab(tab.id);
    if (window.saveWorkspaceState) window.saveWorkspaceState();
  }

  activateTab(tabId) {
    const tab = this.tabs.find(t => t.id === tabId);
    if (!tab) return;

    this.activeTab = tab;
    window.editorAPI.setEditorContent(tab.content, tab.path);
    this.renderTabs();

    document.getElementById('welcome-screen').style.display = 'none';
    document.getElementById('breadcrumb-bar').style.display = 'flex';
    this.updateBreadcrumb(tab);
    if (window.saveWorkspaceState) window.saveWorkspaceState();
  }

  updateBreadcrumb(tab) {
    const breadcrumbPath = document.getElementById('breadcrumb-path');
    if (!breadcrumbPath || !tab) return;
    const rootPath = window.explorer ? window.explorer.rootPath : '';
    let relPath = tab.path;
    if (rootPath && relPath.startsWith(rootPath)) {
      relPath = relPath.substring(rootPath.length).replace(/^[\\/]/, '');
    }
    breadcrumbPath.textContent = relPath.replace(/[/\\]/g, ' > ');
  }

  markCurrentModified() {
    if (!this.activeTab) return;
    const currentContent = window.editorAPI.getEditorContent();
    this.activeTab.modified = currentContent !== this.activeTab.originalContent;
    this.activeTab.content = currentContent;
    this.renderTabs();

    if (this.activeTab.modified && typeof getSettings === 'function' && getSettings().autoSave) {
      if (this.autoSaveTimeout) clearTimeout(this.autoSaveTimeout);
      this.autoSaveTimeout = setTimeout(() => {
        this.saveCurrent();
      }, 1000);
    }
  }

  closeTab(tabId) {
    const idx = this.tabs.findIndex(t => t.id === tabId);
    if (idx === -1) return;

    this.tabs.splice(idx, 1);

    if (this.activeTab && this.activeTab.id === tabId) {
      if (this.tabs.length > 0) {
        const nextIdx = Math.min(idx, this.tabs.length - 1);
        this.activateTab(this.tabs[nextIdx].id);
      } else {
        this.activeTab = null;
        window.editorAPI.setEditorContent('', null);
        document.getElementById('welcome-screen').style.display = 'flex';
        document.getElementById('breadcrumb-bar').style.display = 'none';
      }
    }

    this.renderTabs();
    if (window.saveWorkspaceState) window.saveWorkspaceState();
  }

  async saveCurrent() {
    if (!this.activeTab) return;
    const content = window.editorAPI.getEditorContent();
    const success = await window.api.writeFile(this.activeTab.path, content);
    if (success) {
      this.activeTab.originalContent = content;
      this.activeTab.content = content;
      this.activeTab.modified = false;
      this.renderTabs();
    }
  }

  renderTabs() {
    this.tabsEl.innerHTML = '';
    this.tabs.forEach(tab => {
      const el = document.createElement('div');
      el.className = `tab${tab.id === (this.activeTab && this.activeTab.id) ? ' active' : ''}`;
      el.innerHTML = `
        <span class="tab-icon">${this.getFileIcon(tab.name)}</span>
        <span class="tab-name">${tab.name}</span>
        ${tab.modified ? '<span class="tab-modified"></span>' : ''}
        <span class="tab-close" title="Close">&times;</span>`;

      el.addEventListener('click', (e) => {
        if (e.target.classList.contains('tab-close')) {
          this.closeTab(tab.id);
        } else {
          this.activateTab(tab.id);
        }
      });

      this.tabsEl.appendChild(el);
    });
  }

  getActiveFile() {
    return this.activeTab ? this.activeTab.path : null;
  }
}

window.TabManager = TabManager;
