let gitPanelVisible = false;

function initGit() {
  const refreshBtn = document.getElementById('git-refresh');
  const commitBtn = document.getElementById('git-commit-btn');
  const commitMsg = document.getElementById('git-commit-msg');
  const diffBtn = document.getElementById('git-diff-btn');
  const logBtn = document.getElementById('git-log-btn');

  if (refreshBtn) refreshBtn.addEventListener('click', refreshGitStatus);
  if (commitBtn) commitBtn.addEventListener('click', doGitCommit);
  if (diffBtn) diffBtn.addEventListener('click', showGitDiff);
  if (logBtn) logBtn.addEventListener('click', showGitLog);

  if (commitMsg) {
    commitMsg.addEventListener('keydown', (e) => {
      if (e.key === 'Enter' && e.ctrlKey) doGitCommit();
    });
  }
}

async function refreshGitStatus() {
  const rootPath = window.explorer ? window.explorer.rootPath : null;
  if (!rootPath) return;
  const filesList = document.getElementById('git-files');
  if (!filesList) return;

  filesList.innerHTML = '<div class="git-loading">Loading...</div>';

  try {
    const files = await window.api.gitStatus(rootPath);
    renderGitFiles(files, filesList);
  } catch (e) {
    filesList.innerHTML = `<div class="git-empty">Error: ${e.message}</div>`;
  }
}

function renderGitFiles(files, container) {
  if (!files || files.length === 0) {
    container.innerHTML = '<div class="git-empty">No changes</div>';
    return;
  }

  let html = '';
  for (const f of files) {
    const statusClass = f.status === 'A' ? 'added' : f.status === 'D' ? 'deleted' : f.status === 'M' ? 'modified' : 'other';
    const statusLabel = f.status === 'A' ? 'A' : f.status === 'D' ? 'D' : f.status === 'M' ? 'M' : f.status;
    html += `<div class="git-file" data-path="${f.file}">
      <span class="git-file-status ${statusClass}">${statusLabel}</span>
      <span class="git-file-name">${f.file}</span>
      <button class="git-file-stage" title="Stage">+</button>
    </div>`;
  }
  container.innerHTML = html;

  container.querySelectorAll('.git-file-stage').forEach(btn => {
    btn.addEventListener('click', async (e) => {
      e.stopPropagation();
      const filePath = btn.closest('.git-file').dataset.path;
      const rootPath = window.explorer ? window.explorer.rootPath : null;
      if (rootPath) {
        await window.api.gitAdd(rootPath, filePath);
        refreshGitStatus();
      }
    });
  });

  container.querySelectorAll('.git-file').forEach(el => {
    el.addEventListener('click', async () => {
      const filePath = el.dataset.path;
      const rootPath = window.explorer ? window.explorer.rootPath : null;
      if (rootPath) {
        const diff = await window.api.gitDiff(rootPath, filePath);
        showDiffInEditor(filePath, diff);
      }
    });
  });
}

async function doGitCommit() {
  const rootPath = window.explorer ? window.explorer.rootPath : null;
  if (!rootPath) return;
  const commitMsg = document.getElementById('git-commit-msg');
  const msg = commitMsg.value.trim();
  if (!msg) return;

  const result = await window.api.gitCommit(rootPath, msg);
  if (result.code === 0) {
    commitMsg.value = '';
    refreshGitStatus();
    showGitLog();
  } else {
    const errDiv = document.getElementById('git-output');
    if (errDiv) errDiv.textContent = result.stderr || result.stdout;
  }
}

async function showGitDiff() {
  const rootPath = window.explorer ? window.explorer.rootPath : null;
  if (!rootPath) return;
  const diff = await window.api.gitDiff(rootPath);
  const diffPanel = document.getElementById('git-diff-output');
  const logPanel = document.getElementById('git-log-output');
  if (logPanel) logPanel.style.display = 'none';

  if (diffPanel) {
    diffPanel.style.display = 'block';
    if (!diff.trim()) {
      diffPanel.innerHTML = '<div class="git-empty">No changes</div>';
      return;
    }
    diffPanel.innerHTML = `<pre class="git-diff-content">${escapeHtml(diff)}</pre>`;
  }
}

async function showGitLog() {
  const rootPath = window.explorer ? window.explorer.rootPath : null;
  if (!rootPath) return;
  const log = await window.api.gitLog(rootPath);
  const logPanel = document.getElementById('git-log-output');
  const diffPanel = document.getElementById('git-diff-output');
  if (diffPanel) diffPanel.style.display = 'none';

  if (logPanel) {
    logPanel.style.display = 'block';
    if (!log || log.length === 0) {
      logPanel.innerHTML = '<div class="git-empty">No commits</div>';
      return;
    }
    let html = '';
    for (const entry of log) {
      html += `<div class="git-log-entry">
        <span class="git-log-hash">${entry.hash}</span>
        <span class="git-log-msg">${escapeHtml(entry.message)}</span>
      </div>`;
    }
    logPanel.innerHTML = html;
  }
}

function showDiffInEditor(filePath, diff) {
  const diffPanel = document.getElementById('git-diff-output');
  const logPanel = document.getElementById('git-log-output');
  if (logPanel) logPanel.style.display = 'none';

  if (diffPanel) {
    diffPanel.style.display = 'block';
    if (!diff.trim()) {
      diffPanel.innerHTML = '<div class="git-empty">No changes</div>';
      return;
    }
    diffPanel.innerHTML = `<div class="git-diff-file">${filePath}</div><pre class="git-diff-content">${escapeHtml(diff)}</pre>`;
  }
}

function escapeHtml(text) {
  return text.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}
