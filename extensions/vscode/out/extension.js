"use strict";
//
// Just a thin wrapper for the luau_debugger which is implemented in C++.
//
Object.defineProperty(exports, "__esModule", { value: true });
exports.deactivate = exports.activate = void 0;
const vscode = require("vscode");
const DEBUGGER_IDENTITY = 'luau';
function activate(context) {
    const provider = new LuauConfigurationProvider();
    context.subscriptions.push(vscode.debug.registerDebugConfigurationProvider(DEBUGGER_IDENTITY, provider));
    context.subscriptions.push(vscode.debug.registerDebugAdapterDescriptorFactory(DEBUGGER_IDENTITY, new LuauDebugAdapterServerDescriptorFactory()));
}
exports.activate = activate;
function deactivate() { }
exports.deactivate = deactivate;
class LuauDebugAdapterServerDescriptorFactory {
    createDebugAdapterDescriptor(session, executable) {
        return new vscode.DebugAdapterServer(session.configuration.port, session.configuration.address);
    }
    dispose() { }
}
class LuauConfigurationProvider {
    resolveDebugConfiguration(folder, config, token) {
        return config;
    }
}
//# sourceMappingURL=extension.js.map