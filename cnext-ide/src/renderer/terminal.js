class TerminalManager {
  constructor() {
    this.terminals = {};
    this.activeTerminal = null;
    this.containerEl = document.getElementById('terminal-container');
  }

  async createTerminal() {
    const cwd = window.explorer && window.explorer.rootPath ? window.explorer.rootPath : undefined;
    const id = await window.api.createTerminal(cwd);

    const termDiv = document.createElement('div');
    termDiv.id = `term-${id}`;
    termDiv.style.width = '100%';
    termDiv.style.height = '100%';
    this.containerEl.innerHTML = '';
    this.containerEl.appendChild(termDiv);

    const term = new Terminal({
      theme: {
        background: '#0c0e14',
        foreground: '#d4d7e5',
        cursor: '#7c5ce0',
        cursorAccent: '#0c0e14',
        selectionBackground: '#2a2f45',
        black: '#0c0e14',
        red: '#e06c75',
        green: '#98c379',
        yellow: '#d19a66',
        blue: '#61afef',
        magenta: '#c678dd',
        cyan: '#56b6c2',
        white: '#d4d7e5',
        brightBlack: '#4d5270',
        brightRed: '#e06c75',
        brightGreen: '#c3e88d',
        brightYellow: '#dcb67a',
        brightBlue: '#82aaff',
        brightMagenta: '#c792ea',
        brightCyan: '#89ddff',
        brightWhite: '#ffffff',
      },
      fontFamily: "'JetBrains Mono', 'Cascadia Code', 'Fira Code', 'Consolas', monospace",
      fontSize: 13,
      cursorBlink: true,
      cursorStyle: 'bar',
      allowTransparency: true,
    });

    const fitAddon = new FitAddon.FitAddon();
    term.loadAddon(fitAddon);
    term.open(termDiv);

    setTimeout(() => fitAddon.fit(), 50);

    this.terminals[id] = { term, fitAddon, id };
    this.activeTerminal = this.terminals[id];

    term.onData(data => {
      window.api.terminalWrite(id, data);
    });

    window.api.onTerminalData((termId, data) => {
      if (this.terminals[termId]) {
        this.terminals[termId].term.write(data);
      }
    });

    window.api.onTerminalExit((termId) => {
      if (this.terminals[termId]) {
        this.terminals[termId].term.write('\r\n\x1b[33m[Process exited]\x1b[0m\r\n');
      }
    });

    return id;
  }

  resize() {
    if (this.activeTerminal) {
      this.activeTerminal.fitAddon.fit();
      const { cols, rows } = this.activeTerminal.term;
      window.api.terminalResize(this.activeTerminal.id, cols, rows);
    }
  }

  clear() {
    if (this.activeTerminal) {
      this.activeTerminal.term.clear();
    }
  }
}

window.TerminalManager = TerminalManager;
