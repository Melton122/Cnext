let currentSettings = {};

async function initSettings() {
  currentSettings = await window.api.getSettings();

  const settingsBtn = document.getElementById('settings-btn');
  const settingsModal = document.getElementById('settings-modal');
  const settingsClose = document.getElementById('settings-close');
  const settingsSave = document.getElementById('settings-save');

  if (settingsBtn) {
    settingsBtn.addEventListener('click', openSettings);
  }

  if (settingsClose) {
    settingsClose.addEventListener('click', closeSettings);
  }

  if (settingsSave) {
    settingsSave.addEventListener('click', saveCurrentSettings);
  }

  if (settingsModal) {
    settingsModal.addEventListener('click', (e) => {
      if (e.target === settingsModal) closeSettings();
    });
  }
}

function openSettings() {
  const modal = document.getElementById('settings-modal');
  if (!modal) return;

  document.getElementById('setting-font-size').value = currentSettings.fontSize || 14;
  document.getElementById('setting-tab-size').value = currentSettings.tabSize || 4;
  document.getElementById('setting-font-family').value = currentSettings.fontFamily || "'JetBrains Mono', 'Cascadia Code', 'Fira Code', monospace";
  document.getElementById('setting-compiler').value = currentSettings.compilerPath || 'cnext';
  document.getElementById('setting-shell').value = currentSettings.terminalShell || '';
  document.getElementById('setting-auto-save').checked = currentSettings.autoSave || false;

  modal.classList.add('active');
}

function closeSettings() {
  const modal = document.getElementById('settings-modal');
  if (modal) modal.classList.remove('active');
}

async function saveCurrentSettings() {
  currentSettings = {
    fontSize: parseInt(document.getElementById('setting-font-size').value) || 14,
    tabSize: parseInt(document.getElementById('setting-tab-size').value) || 4,
    fontFamily: document.getElementById('setting-font-family').value || "'JetBrains Mono', 'Cascadia Code', 'Fira Code', monospace",
    compilerPath: document.getElementById('setting-compiler').value || 'cnext',
    terminalShell: document.getElementById('setting-shell').value || '',
    autoSave: document.getElementById('setting-auto-save').checked
  };

  await window.api.saveSettings(currentSettings);
  applySettings();
  closeSettings();
}

function applySettings() {
  if (window.editorAPI) {
    const ed = window.editorAPI.getEditor();
    if (ed && ed.updateOptions) {
      ed.updateOptions({
        fontSize: currentSettings.fontSize,
        tabSize: currentSettings.tabSize,
        fontFamily: currentSettings.fontFamily
      });
    }
  }
  const indentDisplay = document.getElementById('indent-display');
  if (indentDisplay) {
    indentDisplay.textContent = `Spaces: ${currentSettings.tabSize || 4}`;
  }
}

function getSettings() {
  return currentSettings;
}
