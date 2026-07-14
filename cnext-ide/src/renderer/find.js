let currentSearchResults = [];

function initFind() {
  const searchInput = document.getElementById('search-input');
  const searchResults = document.getElementById('search-results');
  const searchClose = document.getElementById('search-close');
  const caseToggle = document.getElementById('search-case-toggle');

  let caseSensitive = false;

  if (searchClose) {
    searchClose.addEventListener('click', () => {
      document.getElementById('find-panel').classList.remove('active');
      document.querySelectorAll('.sidebar-tab').forEach(t => t.classList.remove('active'));
      document.querySelector('[data-panel="explorer"]').classList.add('active');
      document.getElementById('explorer-panel').classList.add('active');
    });
  }

  if (caseToggle) {
    caseToggle.addEventListener('click', () => {
      caseSensitive = !caseSensitive;
      caseToggle.classList.toggle('active', caseSensitive);
      caseToggle.title = caseSensitive ? 'Case Sensitive' : 'Case Insensitive';
    });
  }

  if (searchInput) {
    let debounceTimer = null;
    searchInput.addEventListener('input', () => {
      clearTimeout(debounceTimer);
      debounceTimer = setTimeout(() => performSearch(searchInput.value, caseSensitive), 300);
    });

    searchInput.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') {
        clearTimeout(debounceTimer);
        performSearch(searchInput.value, caseSensitive);
      } else if (e.key === 'Escape') {
        document.getElementById('find-panel').classList.remove('active');
        document.querySelectorAll('.sidebar-tab').forEach(t => t.classList.remove('active'));
        document.querySelector('[data-panel="explorer"]').classList.add('active');
        document.getElementById('explorer-panel').classList.add('active');
      }
    });
  }
}

async function performSearch(query, caseSensitive) {
  const searchResults = document.getElementById('search-results');
  if (!searchResults) return;

  if (!query || query.length < 2) {
    searchResults.innerHTML = '<div class="search-empty">Type at least 2 characters</div>';
    return;
  }

  const rootPath = window.explorer ? window.explorer.rootPath : null;
  if (!rootPath) {
    searchResults.innerHTML = '<div class="search-empty">Open a folder first</div>';
    return;
  }

  searchResults.innerHTML = '<div class="search-empty">Searching...</div>';

  try {
    const results = await window.api.searchInFiles(rootPath, query, caseSensitive);
    currentSearchResults = results;
    renderSearchResults(results, searchResults);
  } catch (e) {
    searchResults.innerHTML = `<div class="search-empty">Error: ${e.message}</div>`;
  }
}

function renderSearchResults(results, container) {
  if (!results || results.length === 0) {
    container.innerHTML = '<div class="search-empty">No results found</div>';
    return;
  }

  const rootPath = window.explorer ? window.explorer.rootPath : '';
  const grouped = {};
  for (const r of results) {
    const relPath = r.file.replace(rootPath, '').replace(/\\/g, '/').replace(/^\//, '');
    if (!grouped[relPath]) grouped[relPath] = [];
    grouped[relPath].push(r);
  }

  let html = `<div class="search-count">${results.length} result${results.length !== 1 ? 's' : ''} in ${Object.keys(grouped).length} file${Object.keys(grouped).length !== 1 ? 's' : ''}</div>`;

  for (const [file, matches] of Object.entries(grouped)) {
    html += `<div class="search-file-group">`;
    html += `<div class="search-file-header" data-path="${matches[0].file}">${file}</div>`;
    for (const m of matches) {
      const escapedText = m.text.replace(/</g, '&lt;').replace(/>/g, '&gt;');
      html += `<div class="search-match" data-file="${m.file}" data-line="${m.line}">
        <span class="search-line-num">${m.line}</span>
        <span class="search-line-text">${escapedText}</span>
      </div>`;
    }
    html += `</div>`;
  }

  container.innerHTML = html;

  container.querySelectorAll('.search-match').forEach(el => {
    el.addEventListener('click', async () => {
      const file = el.dataset.file;
      const line = parseInt(el.dataset.line);
      const name = file.split(/[\\/]/).pop();
      if (window.tabManager) {
        await window.tabManager.openFile(file, name);
        if (line > 0 && window.editorAPI.getEditor()) {
          window.editorAPI.getEditor().revealLineInCenter(line);
          window.editorAPI.getEditor().setPosition({ lineNumber: line, column: 1 });
          window.editorAPI.getEditor().focus();
        }
      }
    });
  });

  container.querySelectorAll('.search-file-header').forEach(el => {
    el.addEventListener('click', async () => {
      const file = el.dataset.path;
      const name = file.split(/[\\/]/).pop();
      if (window.tabManager) {
        await window.tabManager.openFile(file, name);
      }
    });
  });
}
