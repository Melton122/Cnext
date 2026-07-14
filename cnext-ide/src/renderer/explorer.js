class FileExplorer {
  constructor() {
    this.rootPath = null;
    this.treeEl = document.getElementById('file-tree');
    this.expandedDirs = new Set();
  }

  static dirname(filePath) {
    const normalized = filePath.replace(/\\/g, '/');
    const lastSlash = normalized.lastIndexOf('/');
    return lastSlash > 0 ? normalized.substring(0, lastSlash) : filePath;
  }

  async openFolder() {
    const folder = await window.api.openFolder();
    if (!folder) return;
    this.rootPath = folder;
    this.expandedDirs.clear();
    this.expandedDirs.add(folder);
    document.getElementById('project-name').textContent = folder.split(/[\\/]/).pop();
    document.getElementById('welcome-screen').style.display = 'none';
    document.getElementById('breadcrumb-bar').style.display = 'flex';
    await this.refresh();
    if (window.saveWorkspaceState) window.saveWorkspaceState();
    if (window.updateGitBranch) window.updateGitBranch();
  }

  async refresh() {
    if (!this.rootPath) return;
    this.treeEl.innerHTML = '';
    await this.renderDir(this.rootPath, this.treeEl, 0);
  }

  async renderDir(dirPath, container, depth) {
    const entries = await window.api.readDir(dirPath);
    const sorted = entries.sort((a, b) => {
      if (a.isDirectory && !b.isDirectory) return -1;
      if (!a.isDirectory && b.isDirectory) return 1;
      return a.name.localeCompare(b.name);
    });

    for (const entry of sorted) {
      if (entry.name.startsWith('.') || entry.name === 'node_modules' || entry.name === '__pycache__') continue;

      const item = document.createElement('div');
      item.className = 'file-item';
      item.style.paddingLeft = `${8 + depth * 16}px`;

      if (entry.isDirectory) {
        const expanded = this.expandedDirs.has(entry.path);
        item.innerHTML = `<span class="icon">${expanded ? this.folderOpenIcon() : this.folderIcon()}</span><span class="name">${entry.name}</span>`;
        item.addEventListener('click', async () => {
          if (this.expandedDirs.has(entry.path)) {
            this.expandedDirs.delete(entry.path);
          } else {
            this.expandedDirs.add(entry.path);
          }
          await this.refresh();
        });
        container.appendChild(item);

        if (expanded) {
          const childContainer = document.createElement('div');
          childContainer.className = 'file-children';
          container.appendChild(childContainer);
          await this.renderDir(entry.path, childContainer, depth + 1);
        }
      } else {
        const icon = this.getFileIcon(entry.name);
        item.innerHTML = `<span class="icon">${icon}</span><span class="name">${entry.name}</span>`;
        item.addEventListener('click', () => {
          if (window.tabManager) window.tabManager.openFile(entry.path, entry.name);
        });
        item.addEventListener('contextmenu', (e) => {
          e.preventDefault();
          this.showContextMenu(e, entry);
        });
        container.appendChild(item);
      }
    }
  }

  folderIcon() {
    return `<svg width="14" height="14" viewBox="0 0 16 16" fill="none"><path d="M1.5 2h4l1.2 1.2H14v9.8H1.5V2z" fill="#dcb67a"/><path d="M1 4h14v8.5H1V4z" fill="#e8c87a"/></svg>`;
  }

  folderOpenIcon() {
    return `<svg width="14" height="14" viewBox="0 0 16 16" fill="none"><path d="M1.5 2h4l1.2 1.2H14v2H1.5V2z" fill="#dcb67a"/><path d="M0.5 5h15v7.5H0.5V5z" fill="#e8c87a"/></svg>`;
  }

  getFileIcon(name) {
    const ext = name.split('.').pop().toLowerCase();
    const icons = {
      cn:  `<svg width="14" height="14" viewBox="0 0 16 16"><rect x="1" y="1" width="14" height="14" rx="3" fill="url(#cnGrad)"/><defs><linearGradient id="cnGrad" x1="0" y1="0" x2="16" y2="16"><stop offset="0%" stop-color="#7c5ce0"/><stop offset="100%" stop-color="#a88bfa"/></linearGradient></defs><text x="8" y="11.5" text-anchor="middle" font-size="8" font-weight="bold" fill="white" font-family="monospace">C</text></svg>`,
      c:   `<svg width="14" height="14" viewBox="0 0 16 16"><rect x="1" y="1" width="14" height="14" rx="3" fill="#5586c2"/><text x="8" y="11.5" text-anchor="middle" font-size="8" font-weight="bold" fill="white" font-family="monospace">C</text></svg>`,
      h:   `<svg width="14" height="14" viewBox="0 0 16 16"><rect x="1" y="1" width="14" height="14" rx="3" fill="#5586c2"/><text x="8" y="11.5" text-anchor="middle" font-size="8" font-weight="bold" fill="white" font-family="monospace">H</text></svg>`,
      js:  `<svg width="14" height="14" viewBox="0 0 16 16"><rect x="1" y="1" width="14" height="14" rx="3" fill="#d4a72c"/><text x="8" y="11.5" text-anchor="middle" font-size="7" font-weight="bold" fill="white" font-family="monospace">JS</text></svg>`,
      json:`<svg width="14" height="14" viewBox="0 0 16 16"><rect x="1" y="1" width="14" height="14" rx="3" fill="#6b6b6b"/><text x="8" y="11" text-anchor="middle" font-size="6" font-weight="bold" fill="white" font-family="monospace">{ }</text></svg>`,
      md:  `<svg width="14" height="14" viewBox="0 0 16 16"><rect x="1" y="1" width="14" height="14" rx="3" fill="#519aba"/><text x="8" y="11.5" text-anchor="middle" font-size="7" font-weight="bold" fill="white" font-family="monospace">M</text></svg>`,
      py:  `<svg width="14" height="14" viewBox="0 0 16 16"><rect x="1" y="1" width="14" height="14" rx="3" fill="#3572A5"/><text x="8" y="11.5" text-anchor="middle" font-size="7" font-weight="bold" fill="white" font-family="monospace">Py</text></svg>`,
      toml:`<svg width="14" height="14" viewBox="0 0 16 16"><rect x="1" y="1" width="14" height="14" rx="3" fill="#9c6b20"/><text x="8" y="11.5" text-anchor="middle" font-size="6" font-weight="bold" fill="white" font-family="monospace">T</text></svg>`,
      txt: `<svg width="14" height="14" viewBox="0 0 16 16"><rect x="1" y="1" width="14" height="14" rx="3" fill="#6b6b6b"/><text x="8" y="11.5" text-anchor="middle" font-size="7" font-weight="bold" fill="white" font-family="monospace">Tx</text></svg>`,
      rs:  `<svg width="14" height="14" viewBox="0 0 16 16"><rect x="1" y="1" width="14" height="14" rx="3" fill="#dea584"/><text x="8" y="11.5" text-anchor="middle" font-size="7" font-weight="bold" fill="white" font-family="monospace">Rs</text></svg>`,
      yaml:`<svg width="14" height="14" viewBox="0 0 16 16"><rect x="1" y="1" width="14" height="14" rx="3" fill="#cb171e"/><text x="8" y="11.5" text-anchor="middle" font-size="7" font-weight="bold" fill="white" font-family="monospace">Y</text></svg>`,
      yml: `<svg width="14" height="14" viewBox="0 0 16 16"><rect x="1" y="1" width="14" height="14" rx="3" fill="#cb171e"/><text x="8" y="11.5" text-anchor="middle" font-size="7" font-weight="bold" fill="white" font-family="monospace">Y</text></svg>`,
      xml: `<svg width="14" height="14" viewBox="0 0 16 16"><rect x="1" y="1" width="14" height="14" rx="3" fill="#e37933"/><text x="8" y="11.5" text-anchor="middle" font-size="6" font-weight="bold" fill="white" font-family="monospace">&lt;/&gt;</text></svg>`,
      html:`<svg width="14" height="14" viewBox="0 0 16 16"><rect x="1" y="1" width="14" height="14" rx="3" fill="#e34c26"/><text x="8" y="11.5" text-anchor="middle" font-size="6" font-weight="bold" fill="white" font-family="monospace">&lt;&gt;</text></svg>`,
      css: `<svg width="14" height="14" viewBox="0 0 16 16"><rect x="1" y="1" width="14" height="14" rx="3" fill="#563d7c"/><text x="8" y="11.5" text-anchor="middle" font-size="7" font-weight="bold" fill="white" font-family="monospace">#</text></svg>`,
      gitignore:`<svg width="14" height="14" viewBox="0 0 16 16"><rect x="1" y="1" width="14" height="14" rx="3" fill="#f05033"/><text x="8" y="11.5" text-anchor="middle" font-size="6" font-weight="bold" fill="white" font-family="monospace">G</text></svg>`,
    };
    return icons[ext] || `<svg width="14" height="14" viewBox="0 0 16 16"><rect x="1" y="1" width="14" height="14" rx="3" fill="#6b6b6b"/><text x="8" y="11.5" text-anchor="middle" font-size="7" font-weight="bold" fill="white" font-family="monospace">F</text></svg>`;
  }

  showContextMenu(e, entry) {
    this.removeContextMenu();
    const menu = document.createElement('div');
    menu.className = 'context-menu';
    menu.style.left = `${e.clientX}px`;
    menu.style.top = `${e.clientY}px`;

    const items = [
      { label: 'Rename', action: () => this.renameItem(entry) },
      { label: 'Delete', action: () => this.deleteItem(entry) },
      { type: 'separator' },
      { label: 'New File', action: () => this.createFile(entry.isDirectory ? entry.path : FileExplorer.dirname(entry.path)) },
      { label: 'New Folder', action: () => this.createFolder(entry.isDirectory ? entry.path : FileExplorer.dirname(entry.path)) },
    ];

    items.forEach(item => {
      if (item.type === 'separator') {
        const sep = document.createElement('div');
        sep.className = 'context-menu-separator';
        menu.appendChild(sep);
      } else {
        const el = document.createElement('div');
        el.className = 'context-menu-item';
        el.textContent = item.label;
        el.addEventListener('click', () => { this.removeContextMenu(); item.action(); });
        menu.appendChild(el);
      }
    });

    document.body.appendChild(menu);
    document.addEventListener('click', () => this.removeContextMenu(), { once: true });
  }

  removeContextMenu() {
    document.querySelectorAll('.context-menu').forEach(m => m.remove());
  }

  async renameItem(entry) {
    const newName = await this.showInputDialog('Rename', entry.name);
    if (!newName || newName === entry.name) return;
    const parts = entry.path.replace(/\\/g, '/').split('/');
    parts.pop();
    const newPath = parts.join('/') + '/' + newName;
    await window.api.renamePath(entry.path, newPath);
    await this.refresh();
  }

  async deleteItem(entry) {
    await window.api.deletePath(entry.path);
    await this.refresh();
  }

  async createFile(dirPath) {
    const name = await this.showInputDialog('New File', '');
    if (!name) return;
    await window.api.createFile(dirPath + '/' + name);
    await this.refresh();
  }

  async createFolder(dirPath) {
    const name = await this.showInputDialog('New Folder', '');
    if (!name) return;
    await window.api.createDir(dirPath + '/' + name);
    await this.refresh();
  }

  showInputDialog(title, defaultVal) {
    return new Promise((resolve) => {
      const overlay = document.createElement('div');
      overlay.className = 'dialog-overlay';
      overlay.innerHTML = `
        <div class="dialog">
          <h3>${title}</h3>
          <input type="text" value="${defaultVal}" autofocus />
          <div class="dialog-buttons">
            <button class="btn-secondary">Cancel</button>
            <button class="btn-primary">OK</button>
          </div>
        </div>`;

      const input = overlay.querySelector('input');
      const okBtn = overlay.querySelector('.btn-primary');
      const cancelBtn = overlay.querySelector('.btn-secondary');

      okBtn.addEventListener('click', () => { resolve(input.value); overlay.remove(); });
      cancelBtn.addEventListener('click', () => { resolve(null); overlay.remove(); });
      input.addEventListener('keydown', (e) => {
        if (e.key === 'Enter') { resolve(input.value); overlay.remove(); }
        if (e.key === 'Escape') { resolve(null); overlay.remove(); }
      });

      document.body.appendChild(overlay);
      input.focus();
      input.select();
    });
  }
}

window.FileExplorer = FileExplorer;
