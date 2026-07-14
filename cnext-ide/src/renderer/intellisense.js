class IntelliSenseManager {
  constructor() {
    this.stdlibModules = {
      io: ['println', 'print', 'printin', 'input', 'readFile', 'writeFile', 'open', 'close', 'flush'],
      math: ['sqrt', 'abs', 'floor', 'ceil', 'round', 'sin', 'cos', 'tan', 'asin', 'acos', 'atan', 'log', 'log2', 'log10', 'pow', 'min', 'max', 'clamp', 'PI', 'E', 'TAU'],
      os: ['env', 'setenv', 'exec', 'exit', 'sleep', 'platform', 'arch', 'hostname'],
      strings: ['len', 'substr', 'indexOf', 'lastIndexOf', 'replace', 'split', 'join', 'trim', 'trimStart', 'trimEnd', 'upper', 'lower', 'startsWith', 'endsWith', 'contains', 'repeat', 'reverse', 'charAt', 'parseInt', 'parseFloat'],
      json: ['parse', 'stringify', 'prettyPrint'],
      file: ['exists', 'read', 'write', 'append', 'list', 'mkdir', 'remove', 'copy', 'rename', 'isFile', 'isDir', 'size', 'modified'],
      collections: ['list', 'dict', 'set', 'queue', 'stack', 'priorityQueue', 'sortedList', 'pair'],
      time: ['now', 'sleep', 'timestamp', 'format', 'parse', 'micro', 'nano'],
      regex: ['match', 'find', 'findAll', 'replace', 'split', 'test', 'compile'],
      net: ['httpGet', 'httpPost', 'httpPut', 'httpDelete', 'httpPatch', 'tcpConnect', 'tcpListen', 'resolve'],
      crypto: ['hash', 'hmac', 'randomBytes', 'encrypt', 'decrypt', 'sign', 'verify', 'sha256', 'md5'],
      path: ['join', 'dirname', 'basename', 'ext', 'exists', 'abs', 'rel', 'normalize', 'isAbsolute'],
      encoding: ['base64Encode', 'base64Decode', 'hexEncode', 'hexDecode', 'urlEncode', 'urlDecode', 'utf8Encode'],
      process: ['spawn', 'exec', 'exit', 'pid', 'ppid', 'args', 'env', 'cwd'],
      random: ['int', 'float', 'choice', 'shuffle', 'seed', 'bool', 'range', 'gaussian'],
    };

    this.keywords = [
      'if', 'else', 'while', 'for', 'switch', 'case', 'default', 'break', 'continue',
      'return', 'yield', 'match', 'throw', 'try', 'catch', 'finally',
      'var', 'const', 'func', 'class', 'struct', 'enum', 'interface', 'trait',
      'import', 'extends', 'implements', 'override', 'new', 'super', 'this', 'self',
      'extern', 'main', 'test', 'assert', 'typeof', 'as', 'in', 'is', 'null', 'true', 'false',
      'constexpr', 'own', 'coroutine', 'resume', 'async', 'await', 'run_async',
      'type', 'extend', 'macro', 'bench', 'operator',
      'mutex', 'channel', 'spawn', 'thread', 'recv', 'send', 'lock', 'unlock',
    ];

    this.types = [
      'int', 'long', 'float', 'double', 'str', 'bool', 'char', 'byte', 'void',
      'uint', 'ulong', 'ushort', 'ubyte', 'short',
      'iter', 'Option', 'Result', 'List', 'Map', 'Set', 'Queue', 'Stack', 'Tuple',
    ];

    this.builtins = [
      'printin', 'println', 'print', 'input', 'free', 'assert',
      'len', 'str', 'int', 'float', 'bool', 'typeof', 'sizeof',
      'panic', 'unreachable',
    ];
  }
}

window.intelliSenseManager = new IntelliSenseManager();
