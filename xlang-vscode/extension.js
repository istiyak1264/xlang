const vscode = require('vscode');
const { exec } = require('child_process');
const fs = require('fs');
const path = require('path');

let outputChannel;
let statusBarItem;
let diagnosticCollection;

function activate(context) {
    outputChannel = vscode.window.createOutputChannel('XLang');
    diagnosticCollection = vscode.languages.createDiagnosticCollection('xlang');

    statusBarItem = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Right, 100);
    statusBarItem.text = '$(rocket) XLang';
    statusBarItem.tooltip = 'XLang Language Support';
    statusBarItem.command = 'xlang.compileRun';
    statusBarItem.show();

    const cmds = [
        vscode.commands.registerCommand('xlang.compile',    compileFile),
        vscode.commands.registerCommand('xlang.run',        runFile),
        vscode.commands.registerCommand('xlang.compileRun', compileAndRun),
        vscode.commands.registerCommand('xlang.build',      buildProject),
        vscode.commands.registerCommand('xlang.clean',      cleanProject)
    ];

    const completionProvider = vscode.languages.registerCompletionItemProvider(
        'xlang', { provideCompletionItems }, ...['', '.', '(']
    );

    const hoverProvider = vscode.languages.registerHoverProvider('xlang', {
        provideHover(document, position) { return provideHoverInfo(document, position); }
    });

    const formattingProvider = vscode.languages.registerDocumentFormattingEditProvider('xlang', {
        provideDocumentFormattingEdits(document) { return formatDocument(document); }
    });

    context.subscriptions.push(
        ...cmds,
        completionProvider,
        hoverProvider,
        formattingProvider,
        diagnosticCollection,
        outputChannel,
        statusBarItem
    );

    checkCompiler();
}

function deactivate() {
    if (outputChannel)        outputChannel.dispose();
    if (statusBarItem)        statusBarItem.dispose();
    if (diagnosticCollection) diagnosticCollection.dispose();
}

/* ─── helpers ────────────────────────────────────────────────── */

function getConfig() { return vscode.workspace.getConfiguration('xlang'); }

function resolveOutputDir(filePath) {
    const cfg = getConfig();
    let dir = cfg.get('outputDirectory') || '${fileDirname}';
    if (dir === '${fileDirname}') dir = path.dirname(filePath);
    return dir;
}

function executableName(filePath) {
    return path.basename(filePath, path.extname(filePath));
}

/* Returns a Promise that resolves(stdout) or rejects({code, stderr}) */
function runCommand(cmd) {
    return new Promise((resolve, reject) => {
        exec(cmd, { shell: '/bin/sh' }, (err, stdout, stderr) => {
            if (err) reject({ code: err.code, stderr: stderr || err.message });
            else     resolve(stdout);
        });
    });
}

/* ─── compile ────────────────────────────────────────────────── */

async function compileFile() {
    const editor = vscode.window.activeTextEditor;
    if (!editor) { vscode.window.showErrorMessage('No active editor.'); return false; }

    const filePath = editor.document.fileName;
    if (!/\.(x|xl|xlang)$/.test(filePath)) {
        vscode.window.showErrorMessage('Not an XLang file (.x / .xl / .xlang).');
        return false;
    }

    await editor.document.save();

    const cfg      = getConfig();
    const compiler = cfg.get('compilerPath') || 'xlangc';
    const args     = (cfg.get('compilerArgs') || []).join(' ');
    const outDir   = resolveOutputDir(filePath);
    const baseName = executableName(filePath);
    const outPath  = path.join(outDir, baseName);

    outputChannel.clear();
    outputChannel.show(true);
    outputChannel.appendLine(`[XLang] Compiling: ${path.basename(filePath)}`);
    outputChannel.appendLine(`[XLang] Command  : ${compiler} ${args} "${filePath}" -o "${outPath}"`);

    statusBarItem.text = '$(sync~spin) XLang: Compiling…';
    diagnosticCollection.clear();

    const cmd = `"${compiler}" ${args} "${filePath}" -o "${outPath}"`;

    try {
        await runCommand(cmd);
        outputChannel.appendLine(`[XLang] ✓ Success → ${outPath}`);
        statusBarItem.text = '$(check) XLang';
        setTimeout(() => { statusBarItem.text = '$(rocket) XLang'; }, 3000);
        return true;
    } catch ({ stderr }) {
        outputChannel.appendLine(`[XLang] ✗ Compilation failed\n${stderr}`);
        statusBarItem.text = '$(error) XLang';
        setTimeout(() => { statusBarItem.text = '$(rocket) XLang'; }, 4000);
        parseErrors(stderr, editor.document.uri);
        vscode.window.showErrorMessage(`XLang compile failed — see Output panel.`);
        return false;
    }
}

/* ─── run ────────────────────────────────────────────────────── */

async function runFile() {
    const editor = vscode.window.activeTextEditor;
    if (!editor) { vscode.window.showErrorMessage('No active editor.'); return; }

    const filePath = editor.document.fileName;
    const outDir   = resolveOutputDir(filePath);
    const baseName = executableName(filePath);
    const exePath  = path.join(outDir, baseName);

    if (!fs.existsSync(exePath)) {
        vscode.window.showErrorMessage(`Executable not found: ${exePath}. Compile first (F5).`);
        return;
    }

    const terminal = vscode.window.createTerminal(`XLang › ${baseName}`);
    terminal.show();
    terminal.sendText(`cd "${outDir}" && ./${baseName}`);
}

/* ─── compile + run (with proper await) ─────────────────────── */

async function compileAndRun() {
    const ok = await compileFile();
    if (ok) await runFile();
}

/* ─── build project ──────────────────────────────────────────── */

async function buildProject() {
    const folders = vscode.workspace.workspaceFolders;
    if (!folders) { vscode.window.showErrorMessage('No workspace open.'); return; }

    const root    = folders[0].uri.fsPath;
    const srcDir  = path.join(root, 'src');
    if (!fs.existsSync(srcDir)) { vscode.window.showErrorMessage('No src/ directory found.'); return; }

    const files = fs.readdirSync(srcDir).filter(f => /\.(x|xl|xlang)$/.test(f));
    if (!files.length) { vscode.window.showErrorMessage('No XLang source files in src/.'); return; }

    const cfg      = getConfig();
    const compiler = cfg.get('compilerPath') || 'xlangc';
    const projName = path.basename(root);
    const outPath  = path.join(root, projName);
    const fileArgs = files.map(f => `"${path.join(srcDir, f)}"`).join(' ');
    const cmd      = `"${compiler}" ${fileArgs} -o "${outPath}"`;

    outputChannel.clear(); outputChannel.show(true);
    outputChannel.appendLine(`[XLang] Building project: ${projName}`);
    outputChannel.appendLine(`[XLang] ${files.length} source file(s)`);

    try {
        await runCommand(cmd);
        outputChannel.appendLine(`[XLang] ✓ Built → ${outPath}`);
        vscode.window.showInformationMessage(`Build successful: ${outPath}`);
    } catch ({ stderr }) {
        outputChannel.appendLine(`[XLang] ✗ Build failed\n${stderr}`);
        vscode.window.showErrorMessage('Build failed — see Output panel.');
    }
}

/* ─── clean ──────────────────────────────────────────────────── */

async function cleanProject() {
    const folders = vscode.workspace.workspaceFolders;
    if (!folders) return;
    const root = folders[0].uri.fsPath;

    const execs = fs.readdirSync(root).filter(f => {
        const fp = path.join(root, f);
        try {
            return (
                fs.statSync(fp).isFile() &&
                !/\.(x|xl|xlang|c|h|md|json|txt|mk)$/.test(f) &&
                !f.startsWith('.') &&
                fs.accessSync(fp, fs.constants.X_OK) === undefined
            );
        } catch { return false; }
    });

    if (!execs.length) { vscode.window.showInformationMessage('Nothing to clean.'); return; }

    const choice = await vscode.window.showWarningMessage(
        `Delete ${execs.length} executable(s)?`, 'Yes', 'No'
    );
    if (choice !== 'Yes') return;

    for (const f of execs) {
        try {
            fs.unlinkSync(path.join(root, f));
            outputChannel.appendLine(`[XLang] Deleted: ${f}`);
        } catch { outputChannel.appendLine(`[XLang] Could not delete: ${f}`); }
    }
    vscode.window.showInformationMessage('Clean complete.');
}

/* ─── error parsing → Problems panel ────────────────────────── */

function parseErrors(output, uri) {
    const diags = [];
    const re = /\[Error\]\s+line\s+(\d+):\s*(.+)/g;
    let m;
    while ((m = re.exec(output)) !== null) {
        const line = Math.max(0, parseInt(m[1], 10) - 1);
        diags.push(new vscode.Diagnostic(
            new vscode.Range(line, 0, line, 999),
            m[2].trim(),
            vscode.DiagnosticSeverity.Error
        ));
    }
    if (diags.length) diagnosticCollection.set(uri, diags);
}

/* ─── IntelliSense completions ───────────────────────────────── */

function provideCompletionItems() {
    const keywords = [
        'function','return','if','else','while','for','break','skip','done',
        'switch','case','default','import','output','input',
        'int','float','double','string','array','true','false','null'
    ];
    const snippetMap = {
        'function': 'function ${1:name}(${2:params}):\n    ${3:// body}\n    return ${4:0}',
        'if':       'if (${1:condition}):\n    ${2:// body}',
        'while':    'while (${1:condition}):\n    ${2:// body}',
        'for':      'for (${1:int i := 0}; ${2:i < 10}; ${3:i++}):\n    ${4:// body}',
        'switch':   'switch (${1:expr}):\n    case ${2:0}:\n        ${3:// body}\n        break\n    default:\n        ${4:// body}',
        'output':   'output(${1:value})',
        'input':    'input()'
    };

    return keywords.map(kw => {
        const item = new vscode.CompletionItem(kw, vscode.CompletionItemKind.Keyword);
        item.detail = 'XLang keyword';
        if (snippetMap[kw]) {
            item.insertText = new vscode.SnippetString(snippetMap[kw]);
            item.kind = vscode.CompletionItemKind.Snippet;
        }
        return item;
    });
}

/* ─── hover docs ─────────────────────────────────────────────── */

const hoverDocs = {
    'function': '**function** — define a function\n```xlang\nfunction name(int x):\n    return x\n```',
    'if':       '**if** — conditional\n```xlang\nif (x > 0):\n    output(x)\n```',
    'else':     '**else** — alternate branch',
    'while':    '**while** — loop\n```xlang\nwhile (i < 10):\n    i++\n```',
    'for':      '**for** — counted loop\n```xlang\nfor (int i := 0; i < 10; i++):\n    output(i)\n```',
    'output':   '**output(...)** — print to stdout\n```xlang\noutput("hello")\noutput(x, y)\n```',
    'input':    '**input()** — read line from stdin → string\n```xlang\nstring s := input()\n```',
    'int':      '**int** — 64-bit signed integer',
    'float':    '**float** — 32-bit float',
    'double':   '**double** — 64-bit float',
    'string':   '**string** — null-terminated char*\n```xlang\nstring s := "hello"\n```',
    'array':    '**array** — dynamic array\n```xlang\nint[] nums := {1, 2, 3}\n```',
    'return':   '**return** — return a value from function',
    'break':    '**break** — exit loop or switch',
    'skip':     '**skip** — continue (next iteration)',
    'done':     '**done** — exit program (return 0 from main)',
    'switch':   '**switch** — multi-branch on integer\n```xlang\nswitch (x):\n    case 1:\n        output("one")\n        break\n```'
};

function provideHoverInfo(document, position) {
    const range = document.getWordRangeAtPosition(position);
    if (!range) return null;
    const word = document.getText(range);
    if (hoverDocs[word]) return new vscode.Hover(new vscode.MarkdownString(hoverDocs[word]));
    return null;
}

/* ─── formatter ──────────────────────────────────────────────── */

function formatDocument(document) {
    const lines = document.getText().split('\n');
    const out   = [];
    let indent  = 0;
    const IN_STR = /(?:^|[^\\])"(?:[^"\\]|\\.)*"/g; // rough string detector

    for (const raw of lines) {
        const line    = raw.trimEnd();
        const trimmed = line.trim();

        // Strip string contents temporarily to avoid false colon/bracket matches
        const noStr = trimmed.replace(IN_STR, '""');

        // Decrease indent before: else, else if
        if (/^(else\s*if|else)\b/.test(noStr)) indent = Math.max(0, indent - 1);

        if (trimmed) {
            out.push('    '.repeat(indent) + trimmed);
        } else {
            out.push('');
        }

        // Increase indent after lines ending with ':'  (but not :: or case x:)
        if (/:\s*$/.test(noStr) && !/^\s*(case\b.*|default\s*):/.test(noStr)) {
            indent++;
        }
    }

    const full = new vscode.Range(
        document.positionAt(0),
        document.positionAt(document.getText().length)
    );
    return [new vscode.TextEdit(full, out.join('\n'))];
}

/* ─── compiler availability check ───────────────────────────── */

function checkCompiler() {
    const compiler = getConfig().get('compilerPath') || 'xlangc';
    exec(`"${compiler}" --version`, err => {
        if (err) {
            statusBarItem.text    = '$(warning) XLang (compiler not found)';
            statusBarItem.tooltip = `xlangc not found. Run "make install" in your project root.`;
        } else {
            statusBarItem.text    = '$(check) XLang';
            statusBarItem.tooltip = 'XLang compiler ready';
            setTimeout(() => { statusBarItem.text = '$(rocket) XLang'; }, 2000);
        }
    });
}

module.exports = { activate, deactivate };