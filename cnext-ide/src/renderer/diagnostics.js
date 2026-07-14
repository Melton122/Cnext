class DiagnosticsManager {
  constructor() {
    this.problems = [];
    this.problemsEl = document.getElementById('problems-list');
    this.problemsCount = 0;
  }

  async buildCurrentFile() {
    const filePath = window.tabManager ? window.tabManager.getActiveFile() : null;
    if (!filePath) {
      this.showNotification('No file open');
      return;
    }

    if (!filePath.endsWith('.cn')) {
      this.showNotification('Not a Cnext file');
      return;
    }

    this.showNotification('Building...');
    const result = await window.api.runCompiler(['build', filePath]);
    this.parseOutput(result);

    if (result.code === 0) {
      this.showNotification('Build successful');
    }
  }

  async runCurrentFile() {
    const filePath = window.tabManager ? window.tabManager.getActiveFile() : null;
    if (!filePath) {
      this.showNotification('No file open');
      return;
    }

    if (!filePath.endsWith('.cn')) {
      this.showNotification('Not a Cnext file');
      return;
    }

    this.showNotification('Running...');
    const termId = await window.terminalManager.createTerminal();
    const cmd = `cnext run "${filePath}"\r`;
    window.api.terminalWrite(termId, cmd);
  }

  parseOutput(result) {
    this.problems = [];
    window.editorAPI.clearEditorDiagnostics();

    if (result.code === 0 && !result.stderr) {
      this.showNotification('Build successful');
      this.renderProblems();
      this.updateProblemBadge(0);
      return;
    }

    const lines = (result.stderr || '').split('\n');
    const errorRegex = /^(.+?):(\d+):(\d+):\s*(error|warning):\s*(.+)$/;

    for (const line of lines) {
      const match = line.match(errorRegex);
      if (match) {
        this.problems.push({
          file: match[1],
          line: parseInt(match[2]),
          column: parseInt(match[3]),
          severity: match[4],
          message: match[5]
        });
      } else if (line.trim() && (line.includes('error') || line.includes('Error'))) {
        this.problems.push({
          file: '',
          line: 0,
          column: 0,
          severity: 'error',
          message: line.trim()
        });
      }
    }

    const editorErrors = this.problems
      .filter(p => p.line > 0)
      .map(p => ({ line: p.line, column: p.column, message: p.message }));

    window.editorAPI.setEditorDiagnostics(editorErrors);

    this.updateProblemBadge(this.problems.length);
    this.renderProblems();

    if (this.problems.length > 0) {
      this.showProblemsPanel();
    }

    this.showNotification(result.code === 0 ? 'Build with warnings' : 'Build failed');
  }

  updateProblemBadge(count) {
    const panelTab = document.querySelector('.panel-tab[data-panel="problems"]');
    if (panelTab) {
      if (count > 0) {
        panelTab.innerHTML = `Problems <span class="panel-tab-count">${count}</span>`;
      } else {
        panelTab.textContent = 'Problems';
      }
    }
  }

  renderProblems() {
    this.problemsEl.innerHTML = '';

    if (this.problems.length === 0) {
      this.problemsEl.innerHTML = '<div class="problem-empty">No problems detected</div>';
      return;
    }

    this.problems.forEach(problem => {
      const el = document.createElement('div');
      el.className = 'problem-item';
      const icon = problem.severity === 'error' ? '\u2716' : '\u26A0';
      const iconClass = problem.severity === 'error' ? 'error' : 'warning';
      const loc = problem.file ? `${problem.file}:${problem.line}` : '';
      el.innerHTML = `
        <span class="problem-icon ${iconClass}">${icon}</span>
        <span class="problem-msg">${this.escapeHtml(problem.message)}</span>
        <span class="problem-loc">${loc}</span>`;
      el.addEventListener('click', () => {
        if (problem.line > 0 && window.editorAPI.getEditor()) {
          window.editorAPI.getEditor().revealLineInCenter(problem.line);
          window.editorAPI.getEditor().setPosition({ lineNumber: problem.line, column: problem.column || 1 });
          window.editorAPI.getEditor().focus();
        }
      });
      this.problemsEl.appendChild(el);
    });
  }

  escapeHtml(text) {
    return text.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
  }

  showProblemsPanel() {
    document.querySelectorAll('.panel-tab').forEach(t => t.classList.remove('active'));
    document.querySelectorAll('.bottom-content').forEach(c => c.classList.remove('active'));
    document.querySelector('.panel-tab[data-panel="problems"]').classList.add('active');
    document.getElementById('problems-panel').classList.add('active');
    const panel = document.getElementById('bottom-panel');
    if (panel.style.display === 'none') {
      panel.style.display = 'flex';
    }
  }

  showNotification(msg) {
    document.getElementById('status-left').innerHTML = `<span>${msg}</span>`;
  }
}

window.DiagnosticsManager = DiagnosticsManager;
