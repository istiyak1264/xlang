const vscode = require('vscode');
const { exec, spawn } = require('child_process');
const fs = require('fs');
const path = require('path');

let outputChannel;
let statusBarItem;

function activate(context) {
    console.log('XLang extension is now active!');
    
    // Create output channel for compiler messages
    outputChannel = vscode.window.createOutputChannel('XLang');
    
    // Create status bar item
    statusBarItem = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Right, 100);
    statusBarItem.text = "$(rocket) XLang";
    statusBarItem.tooltip = "XLang Language Support";
    statusBarItem.show();
    
    // Register commands
    let compileCmd = vscode.commands.registerCommand('xlang.compile', compileFile);
    let runCmd = vscode.commands.registerCommand('xlang.run', runFile);
    let buildCmd = vscode.commands.registerCommand('xlang.build', buildProject);
    let cleanCmd = vscode.commands.registerCommand('xlang.clean', cleanProject);
    let compileRunCmd = vscode.commands.registerCommand('xlang.compileRun', compileAndRun);
    
    // Register completion provider for IntelliSense
    let completionProvider = vscode.languages.registerCompletionItemProvider('xlang', {
        provideCompletionItems(document, position, token, context) {
            return provideCompletions();
        }
    });
    
    // Register hover provider
    let hoverProvider = vscode.languages.registerHoverProvider('xlang', {
        provideHover(document, position, token) {
            return provideHover(document, position);
        }
    });
    
    // Register document formatting provider
    let formattingProvider = vscode.languages.registerDocumentFormattingEditProvider('xlang', {
        provideDocumentFormattingEdits(document) {
            return formatDocument(document);
        }
    });
    
    // Add all to subscriptions
    context.subscriptions.push(
        compileCmd, runCmd, buildCmd, cleanCmd, compileRunCmd,
        completionProvider, hoverProvider, formattingProvider,
        outputChannel, statusBarItem
    );
    
    // Check if compiler is available
    checkCompiler();
}

function deactivate() {
    if (outputChannel) outputChannel.dispose();
    if (statusBarItem) statusBarItem.dispose();
}

// Compile current file
async function compileFile() {
    const editor = vscode.window.activeTextEditor;
    if (!editor) {
        vscode.window.showErrorMessage('No active editor found');
        return;
    }
    
    const filePath = editor.document.fileName;
    if (!filePath.match(/\.(x|xl|xlang)$/)) {
        vscode.window.showErrorMessage('Not an XLang file');
        return;
    }
    
    const config = vscode.workspace.getConfiguration('xlang');
    const compilerPath = config.get('compilerPath');
    const compilerArgs = config.get('compilerArgs') || [];
    const outputDir = config.get('outputDirectory') || '.';
    
    const baseName = path.basename(filePath, path.extname(filePath));
    const outputFile = path.join(outputDir, baseName);
    
    outputChannel.clear();
    outputChannel.appendLine(`[Compiling] ${filePath}`);
    outputChannel.appendLine(`[Command] ${compilerPath} ${compilerArgs.join(' ')} "${filePath}" -o "${outputFile}"`);
    outputChannel.show();
    
    statusBarItem.text = "$(sync~spin) XLang: Compiling...";
    
    const args = [...compilerArgs, filePath, '-o', outputFile];
    
    exec(`${compilerPath} ${args.join(' ')}`, (error, stdout, stderr) => {
        statusBarItem.text = "$(rocket) XLang";
        
        if (error) {
            outputChannel.appendLine(`[Error] Compilation failed`);
            outputChannel.appendLine(stderr || error.message);
            vscode.window.showErrorMessage(`Compilation failed: ${error.message}`);
            
            // Parse and show errors
            if (stderr) {
                parseAndShowErrors(stderr);
            }
        } else {
            outputChannel.appendLine(`[Success] Compiled to ${outputFile}`);
            vscode.window.showInformationMessage(`Compiled successfully: ${outputFile}`);
            
            statusBarItem.text = "$(check) XLang";
            setTimeout(() => {
                statusBarItem.text = "$(rocket) XLang";
            }, 3000);
        }
    });
}

// Run current file
async function runFile() {
    const editor = vscode.window.activeTextEditor;
    if (!editor) {
        vscode.window.showErrorMessage('No active editor found');
        return;
    }
    
    const filePath = editor.document.fileName;
    const config = vscode.workspace.getConfiguration('xlang');
    const runInTerminal = config.get('runInTerminal');
    const outputDir = config.get('outputDirectory') || '.';
    
    const baseName = path.basename(filePath, path.extname(filePath));
    const executable = path.join(outputDir, baseName);
    
    // Check if executable exists
    if (!fs.existsSync(executable)) {
        vscode.window.showErrorMessage('Executable not found. Please compile first.');
        return;
    }
    
    if (runInTerminal) {
        const terminal = vscode.window.createTerminal(`XLang: ${baseName}`);
        terminal.show();
        terminal.sendText(`cd "${outputDir}" && ./${baseName}`);
    } else {
        exec(`"${executable}"`, (error, stdout, stderr) => {
            if (error) {
                vscode.window.showErrorMessage(`Runtime error: ${error.message}`);
                outputChannel.appendLine(stderr);
            } else {
                outputChannel.appendLine(stdout);
                vscode.window.showInformationMessage('Program executed successfully');
            }
        });
    }
}

// Compile and run
async function compileAndRun() {
    const editor = vscode.window.activeTextEditor;
    if (!editor) return;
    
    await compileFile();
    await runFile();
}

// Build entire project
async function buildProject() {
    const workspaceFolders = vscode.workspace.workspaceFolders;
    if (!workspaceFolders) {
        vscode.window.showErrorMessage('No workspace folder open');
        return;
    }
    
    const rootPath = workspaceFolders[0].uri.fsPath;
    const srcDir = path.join(rootPath, 'src');
    
    if (!fs.existsSync(srcDir)) {
        vscode.window.showErrorMessage('No src/ directory found');
        return;
    }
    
    // Find all .x files
    const files = fs.readdirSync(srcDir).filter(f => f.match(/\.(x|xl|xlang)$/));
    
    if (files.length === 0) {
        vscode.window.showErrorMessage('No XLang source files found');
        return;
    }
    
    const config = vscode.workspace.getConfiguration('xlang');
    const compilerPath = config.get('compilerPath');
    const projectName = path.basename(rootPath);
    const outputFile = path.join(rootPath, projectName);
    
    outputChannel.clear();
    outputChannel.appendLine(`[Building Project] ${projectName}`);
    outputChannel.appendLine(`[Source Files] ${files.length} files`);
    outputChannel.show();
    
    const filesList = files.map(f => `"${path.join(srcDir, f)}"`).join(' ');
    const cmd = `${compilerPath} ${filesList} -o "${outputFile}"`;
    
    exec(cmd, (error, stdout, stderr) => {
        if (error) {
            outputChannel.appendLine(`[Error] Build failed`);
            outputChannel.appendLine(stderr);
            vscode.window.showErrorMessage('Build failed');
        } else {
            outputChannel.appendLine(`[Success] Built to ${outputFile}`);
            vscode.window.showInformationMessage(`Build successful: ${outputFile}`);
        }
    });
}

// Clean build files
async function cleanProject() {
    const workspaceFolders = vscode.workspace.workspaceFolders;
    if (!workspaceFolders) return;
    
    const rootPath = workspaceFolders[0].uri.fsPath;
    const files = fs.readdirSync(rootPath);
    const executables = files.filter(f => {
        const fpath = path.join(rootPath, f);
        try {
            return fs.statSync(fpath).isFile() && 
                   !f.match(/\.(x|xl|xlang|c|h|md|json)$/) &&
                   fs.accessSync(fpath, fs.constants.X_OK);
        } catch(e) {
            return false;
        }
    });
    
    if (executables.length === 0) {
        vscode.window.showInformationMessage('No build files to clean');
        return;
    }
    
    const choice = await vscode.window.showWarningMessage(
        `Delete ${executables.length} executable(s)?`, 
        'Yes', 'No'
    );
    
    if (choice === 'Yes') {
        for (const file of executables) {
            try {
                fs.unlinkSync(path.join(rootPath, file));
                outputChannel.appendLine(`[Deleted] ${file}`);
            } catch(err) {
                outputChannel.appendLine(`[Error] Could not delete ${file}`);
            }
        }
        vscode.window.showInformationMessage('Clean completed');
    }
}

// Provide IntelliSense completions
function provideCompletions() {
    const completions = [];
    
    const keywords = [
        'function', 'return', 'if', 'else', 'while', 'for', 
        'break', 'skip', 'done', 'switch', 'case', 'default',
        'import', 'output', 'input', 'int', 'float', 'double', 
        'string', 'array', 'true', 'false', 'null'
    ];
    
    for (const keyword of keywords) {
        const item = new vscode.CompletionItem(keyword, vscode.CompletionItemKind.Keyword);
        item.insertText = keyword;
        item.detail = 'XLang keyword';
        completions.push(item);
    }
    
    return completions;
}

// Provide hover information
function provideHover(document, position) {
    const wordRange = document.getWordRangeAtPosition(position);
    if (!wordRange) return null;
    
    const word = document.getText(wordRange);
    
    const hoverInfo = {
        'function': 'Defines a function\n```xlang\nfunction name(parameters):\n    # function body\n```',
        'if': 'Conditional statement\n```xlang\nif (condition):\n    # code\nelse:\n    # code\n```',
        'while': 'While loop\n```xlang\nwhile (condition):\n    # loop body\n```',
        'for': 'For loop\n```xlang\nfor (init; condition; update):\n    # loop body\n```',
        'output': 'Prints values to console\n```xlang\noutput(value1, value2, ...)\n```',
        'input': 'Reads input from user\n```xlang\nstring name := input()\n```',
        'int': 'Integer type (32-bit signed)',
        'float': 'Floating-point type (32-bit)',
        'double': 'Double-precision float (64-bit)',
        'string': 'String type (null-terminated character array)',
        'array': 'Array type\n```xlang\nint[] numbers := {1, 2, 3}\n```'
    };
    
    if (hoverInfo[word]) {
        return new vscode.Hover(hoverInfo[word]);
    }
    
    return null;
}

// Format document
function formatDocument(document) {
    const text = document.getText();
    const lines = text.split('\n');
    const formattedLines = [];
    let indentLevel = 0;
    
    for (let i = 0; i < lines.length; i++) {
        let line = lines[i];
        const trimmed = line.trim();
        
        // Decrease indent for closing blocks
        if (trimmed === 'else:' || trimmed === '}' || trimmed.startsWith('else if')) {
            indentLevel = Math.max(0, indentLevel - 1);
        }
        
        // Add indentation
        if (trimmed.length > 0) {
            formattedLines.push('    '.repeat(indentLevel) + trimmed);
        } else {
            formattedLines.push('');
        }
        
        // Increase indent after colon
        if (trimmed.endsWith(':')) {
            indentLevel++;
        }
    }
    
    const formattedText = formattedLines.join('\n');
    const fullRange = new vscode.Range(
        document.positionAt(0),
        document.positionAt(text.length)
    );
    
    return [new vscode.TextEdit(fullRange, formattedText)];
}

// Parse and show errors in problems panel
function parseAndShowErrors(output) {
    const diagnosticCollection = vscode.languages.createDiagnosticCollection('xlang');
    const diagnostics = [];
    const errorPattern = /\[Error\] line (\d+): (.*)$/gm;
    
    let match;
    while ((match = errorPattern.exec(output)) !== null) {
        const lineNum = parseInt(match[1]) - 1;
        const message = match[2];
        
        const diagnostic = new vscode.Diagnostic(
            new vscode.Range(lineNum, 0, lineNum, 1000),
            message,
            vscode.DiagnosticSeverity.Error
        );
        diagnostics.push(diagnostic);
    }
    
    if (vscode.window.activeTextEditor && diagnostics.length > 0) {
        diagnosticCollection.set(vscode.window.activeTextEditor.document.uri, diagnostics);
    }
}

// Check if compiler is available
function checkCompiler() {
    const config = vscode.workspace.getConfiguration('xlang');
    const compilerPath = config.get('compilerPath');
    
    exec(`${compilerPath} --version`, (error) => {
        if (error) {
            statusBarItem.text = "$(warning) XLang (not found)";
            statusBarItem.tooltip = "XLang compiler not found. Run 'make install' first";
            vscode.window.showWarningMessage('XLang compiler not found. Please install it first.');
        } else {
            statusBarItem.text = "$(check) XLang";
            statusBarItem.tooltip = "XLang compiler ready";
        }
    });
}

module.exports = { activate, deactivate };