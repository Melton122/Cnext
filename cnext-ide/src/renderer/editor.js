let editor = null;
let monacoInstance = null;

async function setupMonacoWorker() {
  try {
    const workerCode = await window.api.readWorkerScript();
    if (workerCode) {
      const blob = new Blob([workerCode], { type: 'application/javascript' });
      const url = URL.createObjectURL(blob);
      window.MonacoEnvironment = { getWorkerUrl: () => url };
    }
  } catch (e) {
    // Monaco will fall back to main thread
  }
}

const CNEXT_COLORS = {
  bg:             '#0c0e14',
  bgEditor:       '#0f1118',
  bgSidebar:      '#12141c',
  bgToolbar:      '#14161f',
  fg:             '#d4d7e5',
  fgDim:          '#6b7194',
  fgMuted:        '#3d4263',
  keyword:        '#c678dd',
  keywordFlow:    '#e06c75',
  type:           '#e5c07b',
  string:         '#98c379',
  number:         '#d19a66',
  comment:        '#5c6370',
  function:       '#61afef',
  variable:       '#e5e5e5',
  operator:       '#56b6c2',
  bracket:        '#abb2bf',
  constant:       '#d19a66',
  class:          '#e5c07b',
  enum:           '#e5c07b',
  builtin:        '#56b6c2',
  parameter:      '#e06c75',
  property:       '#e06c75',
  namespace:      '#61afef',
  annotation:     '#d19a66',
  regex:          '#98c379',
  tag:            '#e06c75',
  accent:         '#7c5ce0',
  accent2:        '#61afef',
  error:          '#e06c75',
  warning:        '#d19a66',
  selection:      '#2a2f45',
  lineHighlight:  '#14161f',
};

const cnextTheme = {
  base: 'vs-dark',
  inherit: false,
  rules: [
    { token: '', foreground: CNEXT_COLORS.fg },
    { token: 'comment', foreground: CNEXT_COLORS.comment, fontStyle: 'italic' },
    { token: 'keyword', foreground: CNEXT_COLORS.keyword },
    { token: 'keyword.flow', foreground: CNEXT_COLORS.keywordFlow },
    { token: 'type.identifier', foreground: CNEXT_COLORS.type },
    { token: 'type.builtin', foreground: CNEXT_COLORS.builtin },
    { token: 'string', foreground: CNEXT_COLORS.string },
    { token: 'string.escape', foreground: CNEXT_COLORS.string, fontStyle: 'bold' },
    { token: 'number', foreground: CNEXT_COLORS.number },
    { token: 'operator', foreground: CNEXT_COLORS.operator },
    { token: 'delimiter', foreground: CNEXT_COLORS.bracket },
    { token: 'delimiter.dot', foreground: CNEXT_COLORS.fgDim },
    { token: 'identifier', foreground: CNEXT_COLORS.fg },
    { token: 'function', foreground: CNEXT_COLORS.function },
    { token: 'variable', foreground: CNEXT_COLORS.variable },
    { token: 'constant', foreground: CNEXT_COLORS.constant },
    { token: 'annotation', foreground: CNEXT_COLORS.annotation },
    { token: 'tag', foreground: CNEXT_COLORS.tag },
    { token: 'metatag', foreground: CNEXT_COLORS.annotation },
    { token: 'regexp', foreground: CNEXT_COLORS.regex },
    { token: 'string.invalid', foreground: CNEXT_COLORS.error },
    { token: 'class', foreground: CNEXT_COLORS.class },
    { token: 'enum', foreground: CNEXT_COLORS.enum },
    { token: 'namespace', foreground: CNEXT_COLORS.namespace },
    { token: 'parameter', foreground: CNEXT_COLORS.parameter },
    { token: 'property', foreground: CNEXT_COLORS.property },
  ],
  colors: {
    'editor.background':                CNEXT_COLORS.bgEditor,
    'editor.foreground':                CNEXT_COLORS.fg,
    'editorLineNumber.foreground':      CNEXT_COLORS.fgMuted,
    'editorLineNumber.activeForeground':CNEXT_COLORS.fgDim,
    'editor.lineHighlightBackground':   CNEXT_COLORS.lineHighlight,
    'editor.selectionBackground':       CNEXT_COLORS.selection,
    'editorCursor.foreground':          CNEXT_COLORS.accent,
    'editor.wordHighlightBackground':   '#2a2f4588',
    'editorBracketMatch.background':    '#7c5ce033',
    'editorBracketMatch.border':        '#7c5ce0aa',
    'editorIndentGuide.background':     '#2a2f4522',
    'editorIndentGuide.activeBackground':'#2a2f4566',
    'editorWidget.background':          CNEXT_COLORS.bgSidebar,
    'editorWidget.border':              '#2a2f45',
    'editorSuggestWidget.background':   CNEXT_COLORS.bgSidebar,
    'editorSuggestWidget.border':       '#2a2f45',
    'editorSuggestWidget.selectedBackground': '#2a2f45',
    'editorHoverWidget.background':     CNEXT_COLORS.bgSidebar,
    'editorHoverWidget.border':         '#2a2f45',
    'minimap.background':               CNEXT_COLORS.bg,
    'scrollbarSlider.background':       '#2a2f4588',
    'scrollbarSlider.hoverBackground':  '#2a2f45aa',
    'scrollbarSlider.activeBackground': '#3d426388',
  }
};

const cnextLanguage = {
  defaultToken: '',
  tokenPostfix: '.cn',
  keywords: [
    'if','else','while','for','switch','case','default','break','continue',
    'return','yield','match','throw','try','catch','finally',
    'var','const','func','class','struct','enum','interface','trait',
    'import','extends','implements','override','new','super','this','self',
    'extern','main','test','assert','typeof','as','in','is','null','true','false',
    'constexpr','own','coroutine','resume','async','await','run_async',
    'operator','extend','macro','bench','mutex','channel','spawn','thread',
    'recv','send','type',
  ],
  controlKeywords: ['if','else','while','for','switch','case','default','break','continue','return','match','throw','try','catch','finally'],
  declarationKeywords: ['var','const','func','class','struct','enum','interface','trait','import','extends','implements','override','new','extern','main','test','type','macro','coroutine','async'],
  modifierKeywords: ['override','extern','constexpr','own','async','await'],
  typeKeywords: [
    'int','long','float','double','str','bool','char','byte','void',
    'uint','ulong','ushort','ubyte','short',
    'iter','Option','Result',
  ],
  builtinTypes: ['Option','Result','List','Map','Set','Queue','Stack','Tuple','iter'],
  builtins: ['printin','println','print','input','free','assert','len','str','int','float','bool','typeof','sizeof'],
  operators: ['=','+=','-=','*=','/=','==','!=','<','>','<=','>=','&&','||','!','++','--','+','-','*','/','%','??','?.','->','=>','$','...'],
  symbols: /[=><!~?:&|+\-*\/\^%]+/,
  digits: /\d+(_+\d+)*/,
  tokenizer: {
    root: [
      [/[a-z_$][\w$]*/, { cases: {
        '@controlKeywords': 'keyword.flow',
        '@declarationKeywords': 'keyword',
        '@modifierKeywords': 'keyword',
        '@typeKeywords': 'type.identifier',
        '@keywords': 'keyword',
        '@builtins': 'type.builtin',
        '@builtinTypes': 'type.identifier',
        '@default': 'identifier'
      }}],
      [/[A-Z][\w$]*/, 'type.identifier'],
      { include: '@whitespace' },
      [/[{}()\[\]]/, '@brackets'],
      [/@symbols/, { cases: { '@operators': 'operator', '@default': '' } }],
      [/,/, 'delimiter'],
      [/\./, 'delimiter.dot'],
      [/\d+\.\d+/, 'number.float'],
      [/\d+/, 'number'],
      [/"([^"\\]|\\.)*$/, 'string.invalid'],
      [/"/, 'string', '@string'],
      [/'([^'\\]|\\.)*$/, 'string.invalid'],
      [/'/, 'string', '@singlestring'],
      [/#[\w]+/, 'annotation'],
      [/@@[\w]+/, 'annotation'],
    ],
    whitespace: [
      [/[ \t\r\n]+/, 'white'],
      [/\/\/.*$/, 'comment'],
      [/\/\*/, 'comment', '@comment'],
    ],
    comment: [
      [/[^\/*]+/, 'comment'],
      [/\*\//, 'comment', '@pop'],
      [/[/*]/, 'comment'],
    ],
    string: [
      [/\{[\w.]+\}/, 'string.escape'],
      [/[^\\"]+/, 'string'],
      [/\\./, 'string.escape'],
      [/"/, 'string', '@pop'],
    ],
    singlestring: [
      [/\{[\w.]+\}/, 'string.escape'],
      [/[^\\']+/, 'string'],
      [/\\./, 'string.escape'],
      [/'/, 'string', '@pop'],
    ],
  }
};

const cnextCompletionItems = [
  { label: 'func', kind: 14, insertText: 'func ${1:name}(${2:params}) -> ${3:type} {\n\t$0\n}', insertTextRules: 4, documentation: 'Function declaration' },
  { label: 'class', kind: 14, insertText: 'class ${1:Name} {\n\t${2:type} ${3:field}\n\n\tfunc new(${4:params}) {\n\t\t$5\n\t}\n\n\t$0\n}', insertTextRules: 4, documentation: 'Class declaration' },
  { label: 'struct', kind: 14, insertText: 'struct ${1:Name} {\n\t${2:type} ${3:field}\n\t$0\n}', insertTextRules: 4, documentation: 'Struct declaration' },
  { label: 'enum', kind: 14, insertText: 'enum ${1:Name} {\n\t${2:VARIANT1},\n\t${3:VARIANT2}\n}', insertTextRules: 4, documentation: 'Enum declaration' },
  { label: 'interface', kind: 14, insertText: 'interface ${1:Name} {\n\tfunc ${2:method}(${3:params}) -> ${4:type}\n\t$0\n}', insertTextRules: 4, documentation: 'Interface declaration' },
  { label: 'trait', kind: 14, insertText: 'trait ${1:Name} {\n\tfunc ${2:method}(${3:params}) -> ${4:type}\n\t$0\n}', insertTextRules: 4, documentation: 'Trait declaration' },
  { label: 'if', kind: 14, insertText: 'if ${1:condition} {\n\t$0\n}', insertTextRules: 4, documentation: 'If statement' },
  { label: 'ifelse', kind: 14, insertText: 'if ${1:condition} {\n\t$2\n} else {\n\t$0\n}', insertTextRules: 4, documentation: 'If-else statement' },
  { label: 'while', kind: 14, insertText: 'while ${1:condition} {\n\t$0\n}', insertTextRules: 4, documentation: 'While loop' },
  { label: 'for', kind: 14, insertText: 'for ${1:int} ${2:i} = ${3:0}; ${2:i} < ${4:limit}; ${2:i} = ${2:i} + 1 {\n\t$0\n}', insertTextRules: 4, documentation: 'For loop' },
  { label: 'forin', kind: 14, insertText: 'for ${1:var} ${2:item} in ${3:collection} {\n\t$0\n}', insertTextRules: 4, documentation: 'For-in loop' },
  { label: 'switch', kind: 14, insertText: 'switch ${1:expression} {\n\tcase ${2:value}:\n\t\t$3\n\tdefault:\n\t\t$0\n}', insertTextRules: 4, documentation: 'Switch statement' },
  { label: 'match', kind: 14, insertText: 'match ${1:expression} {\n\t${2:pattern} => ${3:result}\n\t_ => ${4:default}\n}', insertTextRules: 4, documentation: 'Match expression' },
  { label: 'try', kind: 14, insertText: 'try {\n\t$1\n} catch (${2:str} ${3:err}) {\n\t$0\n}', insertTextRules: 4, documentation: 'Try-catch block' },
  { label: 'trycatchfinally', kind: 14, insertText: 'try {\n\t$1\n} catch (${2:str} ${3:err}) {\n\t$4\n} finally {\n\t$0\n}', insertTextRules: 4, documentation: 'Try-catch-finally block' },
  { label: 'import', kind: 14, insertText: 'import ${1:module}', insertTextRules: 4, documentation: 'Import statement' },
  { label: 'var', kind: 14, insertText: 'var ${1:name} = ${2:value}', insertTextRules: 4, documentation: 'Variable declaration with type inference' },
  { label: 'const', kind: 14, insertText: 'const ${1:type} ${2:name} = ${3:value}', insertTextRules: 4, documentation: 'Constant declaration' },
  { label: 'lambda', kind: 14, insertText: '(${1:params}) => ${2:expression}', insertTextRules: 4, documentation: 'Lambda expression' },
  { label: 'async', kind: 14, insertText: 'async func ${1:name}(${2:params}) -> ${3:type} {\n\t$0\n}', insertTextRules: 4, documentation: 'Async function' },
  { label: 'coroutine', kind: 14, insertText: 'coroutine func ${1:name}(${2:params}) -> iter<${3:type}> {\n\t$0\n}', insertTextRules: 4, documentation: 'Coroutine function' },
  { label: 'test', kind: 14, insertText: 'test "${1:description}" {\n\t$0\n}', insertTextRules: 4, documentation: 'Test block' },
  { label: 'type', kind: 14, insertText: 'type ${1:Alias} = ${2:Original}', insertTextRules: 4, documentation: 'Type alias (v8.0)' },
  { label: 'extend', kind: 14, insertText: 'extend ${1:Type} {\n\tfunc ${2:method}(${3:params}) -> ${4:type} {\n\t\t$0\n\t}\n}', insertTextRules: 4, documentation: 'Extension method' },
  { label: 'macro', kind: 14, insertText: 'macro ${1:name}(${2:params}) {\n\t$0\n}', insertTextRules: 4, documentation: 'Macro declaration' },
  { label: 'extern', kind: 14, insertText: 'extern "C" func ${1:name}(${2:params}) -> ${3:type}', insertTextRules: 4, documentation: 'External C function' },
  { label: 'main', kind: 14, insertText: 'main {\n\t$0\n}', insertTextRules: 4, documentation: 'Main entry point' },
  { label: 'option', kind: 15, insertText: 'Option<${1:T}>', insertTextRules: 4, documentation: 'Optional type wrapper' },
  { label: 'result', kind: 15, insertText: 'Result<${1:T}, ${2:E}>', insertTextRules: 4, documentation: 'Result type for error handling' },
  { label: 'iter', kind: 15, insertText: 'iter<${1:T}>', insertTextRules: 4, documentation: 'Iterator type' },
  { label: 'list', kind: 15, insertText: 'List<${1:T}>', insertTextRules: 4, documentation: 'Dynamic array type' },
  { label: 'map', kind: 15, insertText: 'Map<${1:K}, ${2:V}>', insertTextRules: 4, documentation: 'HashMap type' },
  { label: 'own', kind: 14, insertText: 'own ${1:type} ${2:name}', insertTextRules: 4, documentation: 'Ownership marker' },
  { label: 'spawn', kind: 14, insertText: 'spawn ${1:func}(${2:args})', insertTextRules: 4, documentation: 'Spawn a thread' },
  { label: 'mutex', kind: 14, insertText: 'mutex ${1:name}', insertTextRules: 4, documentation: 'Mutex declaration' },
  { label: 'channel', kind: 14, insertText: 'channel<${1:T}>(${2:capacity})', insertTextRules: 4, documentation: 'Channel declaration' },
];

function initEditor() {
  return new Promise(async (resolve) => {
    await setupMonacoWorker();

    const monaco = window.monaco;
    if (!monaco) {
      console.error('Monaco not loaded');
      resolve(null);
      return;
    }
    monacoInstance = monaco;

    monaco.languages.register({ id: 'cnext' });
    monaco.languages.setMonarchTokensProvider('cnext', cnextLanguage);
    monaco.languages.setLanguageConfiguration('cnext', {
      comments: { lineComment: '//', blockComment: ['/*', '*/'] },
      brackets: [['{', '}'], ['(', ')'], ['[', ']']],
      autoClosingPairs: [
        { open: '{', close: '}' },
        { open: '(', close: ')' },
        { open: '[', close: ']' },
        { open: '"', close: '"' },
        { open: "'", close: "'" },
      ],
      surroundingPairs: [
        { open: '{', close: '}' },
        { open: '(', close: ')' },
        { open: '[', close: ']' },
        { open: '"', close: '"' },
        { open: "'", close: "'" },
      ],
      indentationRules: {
        increaseIndentPattern: /^\s*(func|class|struct|enum|interface|trait|if|else|while|for|switch|case|test|try|catch|finally|main|macro|coroutine|extend|type)\b.*\{/,
        decreaseIndentPattern: /^\s*\}/,
      }
    });

    monaco.languages.registerCompletionItemProvider('cnext', {
      provideCompletionItems: (model, position) => {
        const word = model.getWordUntilPosition(position);
        const range = { startLineNumber: position.lineNumber, startColumn: word.startColumn, endLineNumber: position.lineNumber, endColumn: word.endColumn };

        let dynamicItems = [];
        if (window.intelliSenseManager && window.intelliSenseManager.stdlibModules) {
          for (const [module, funcs] of Object.entries(window.intelliSenseManager.stdlibModules)) {
            for (const func of funcs) {
              dynamicItems.push({
                label: `${module}.${func}`,
                kind: 3,
                insertText: `${module}.${func}(\${1:args})`,
                insertTextRules: 4,
                documentation: `Function ${func} from ${module} module`,
                range: range
              });
            }
          }
        }

        return { suggestions: [...cnextCompletionItems.map(item => ({ ...item, range })), ...dynamicItems] };
      }
    });

    monaco.editor.defineTheme('cnext-dark', cnextTheme);

    editor = monaco.editor.create(document.getElementById('editor'), {
      value: '',
      language: 'cnext',
      theme: 'cnext-dark',
      automaticLayout: true,
      fontSize: 14,
      fontFamily: "'Cascadia Code', 'Fira Code', 'JetBrains Mono', 'Consolas', monospace",
      fontLigatures: true,
      minimap: { enabled: true, maxColumn: 80, renderCharacters: false, showSlider: 'mouseover' },
      scrollBeyondLastLine: false,
      renderWhitespace: 'selection',
      tabSize: 4,
      insertSpaces: true,
      wordWrap: 'off',
      lineNumbers: 'on',
      glyphMargin: true,
      folding: true,
      bracketPairColorization: { enabled: true },
      guides: { bracketPairs: true, indentation: true },
      cursorBlinking: 'smooth',
      cursorSmoothCaretAnimation: 'on',
      smoothScrolling: true,
      padding: { top: 10, bottom: 10 },
      renderLineHighlight: 'all',
      occurrencesHighlight: 'singleFile',
      selectionHighlight: true,
      links: true,
      colorDecorators: true,
      contextmenu: true,
      mouseWheelZoom: true,
      suggest: { showMethods: true, showFunctions: true, showConstructors: true, showFields: true, showVariables: true, showClasses: true, showStructs: true, showInterfaces: true, showModules: true, showProperties: true, showEvents: true, showOperators: true, showUnits: true, showValues: true, showConstants: true, showEnums: true, showEnumMembers: true, showKeywords: true, showWords: true, showColors: true, showFiles: true, showReferences: true, showFolders: true, showTypeParameters: true, showSnippets: true },
      quickSuggestions: { other: true, comments: true, strings: true },
    });

    editor.onDidChangeCursorPosition((e) => {
      document.getElementById('cursor-position').textContent = `Ln ${e.position.lineNumber}, Col ${e.position.column}`;
    });

    editor.onDidChangeModelContent(() => {
      if (window.tabManager) window.tabManager.markCurrentModified();
    });

    resolve(editor);
  });
}

function setEditorContent(content, filePath) {
  if (!editor || !monacoInstance) return;
  const ext = filePath ? filePath.split('.').pop() : 'cn';
  const lang = ext === 'cn' ? 'cnext' : ext === 'c' || ext === 'h' ? 'c' : ext === 'js' ? 'javascript' : ext === 'json' ? 'json' : ext === 'md' ? 'markdown' : ext === 'py' ? 'python' : ext === 'toml' ? 'ini' : 'plaintext';
  const model = monacoInstance.editor.createModel(content, lang);
  editor.setModel(model);
}

function getEditorContent() {
  return editor ? editor.getValue() : '';
}

function setEditorDiagnostics(errors) {
  if (!monacoInstance || !editor) return;
  const model = editor.getModel();
  const markers = errors.map(err => ({
    severity: monacoInstance.MarkerSeverity.Error,
    startLineNumber: err.line || 1,
    startColumn: err.column || 1,
    endLineNumber: err.line || 1,
    endColumn: err.column ? err.column + 1 : 100,
    message: err.message
  }));
  monacoInstance.editor.setModelMarkers(model, 'cnext', markers);
}

function clearEditorDiagnostics() {
  if (!monacoInstance || !editor) return;
  monacoInstance.editor.setModelMarkers(editor.getModel(), 'cnext', []);
}

window.editorAPI = { initEditor, setEditorContent, getEditorContent, setEditorDiagnostics, clearEditorDiagnostics, getEditor: () => editor };
