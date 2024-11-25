"use strict";
//
// Just a thin wrapper for the luau_debugger which is implemented in C++.
//
Object.defineProperty(exports, "__esModule", { value: true });
exports.deactivate = exports.activate = void 0;
const vscode = require("vscode");
const path = require("path");
const fs = require("fs");
const child_process = require("child_process");
const DEBUGGER_IDENTITY = 'luau';
function activate(context) {
    const provider = new LuauConfigurationProvider();
    context.subscriptions.push(vscode.debug.registerDebugConfigurationProvider(DEBUGGER_IDENTITY, provider));
    context.subscriptions.push(vscode.debug.registerDebugAdapterDescriptorFactory(DEBUGGER_IDENTITY, new LuauDebugAdapterServerDescriptorFactory()));
}
exports.activate = activate;
function deactivate() { }
exports.deactivate = deactivate;
function killDebugger() {
    try {
        child_process.execSync(`taskkill /f /im luaud.exe`);
    }
    catch (e) { }
}
async function spawnLuauDebugger(session) {
    return new Promise((resolve, reject) => {
        const extension_folder = vscode.extensions.getExtension('sssooonnnggg.luau-debugger').extensionPath;
        const debugger_path = extension_folder + '/debugger/luaud.exe';
        if (vscode.workspace.workspaceFolders?.length == 0) {
            vscode.window.showErrorMessage('No workspace folder is opened.');
            return;
        }
        const workspaces = vscode.workspace.workspaceFolders;
        const entry_path = path.resolve(workspaces[0].uri.fsPath, session.configuration.program);
        if (!fs.existsSync(entry_path)) {
            vscode.window.showErrorMessage(`lua file "${entry_path}" does not exist.`);
            return;
        }
        killDebugger();
        const process = child_process.spawn(debugger_path, [session.configuration.port, entry_path], { detached: true, stdio: 'pipe' });
        let timeout = setTimeout(() => {
            killDebugger();
            resolve(null);
        }, 1000 * 60);
        process.stderr.on('data', data => {
            console.log(data);
        });
        process.stdout.on('data', data => {
            const waiting = 'wait for client connection';
            console.log(data);
            if (data.toString().includes(waiting)) {
                clearTimeout(timeout);
                resolve(process);
            }
        });
        process.on('exit', code => {
            console.log(`luau debugger exited with code ${code}`);
        });
    });
}
class LuauDebugAdapterServerDescriptorFactory {
    constructor() {
        this.process = null;
    }
    async createDebugAdapterDescriptor(session, executable) {
        if (session.configuration.request == 'launch') {
            this.process = await spawnLuauDebugger(session);
            if (!this.process) {
                vscode.window.showErrorMessage('Failed to start luau debugger.');
                throw new Error('Failed to start luau debugger.');
            }
        }
        return new vscode.DebugAdapterServer(session.configuration.port, session.configuration.address);
    }
    dispose() {
        this.process?.kill();
    }
}
class LuauConfigurationProvider {
    resolveDebugConfiguration(folder, config, token) {
        return config;
    }
}
//# sourceMappingURL=extension.js.map