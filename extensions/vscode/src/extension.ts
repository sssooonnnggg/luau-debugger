//
// Just a thin wrapper for the luau_debugger which is implemented in C++.
//

import * as vscode from 'vscode';
import { WorkspaceFolder, DebugConfiguration, ProviderResult, CancellationToken } from 'vscode';

const DEBUGGER_IDENTITY = 'luau';

export function activate(context: vscode.ExtensionContext) {
    const provider = new LuauConfigurationProvider();
    context.subscriptions.push(vscode.debug.registerDebugConfigurationProvider(DEBUGGER_IDENTITY, provider));
    context.subscriptions.push(vscode.debug.registerDebugAdapterDescriptorFactory(DEBUGGER_IDENTITY, new LuauDebugAdapterServerDescriptorFactory()));
}

export function deactivate() {}

class LuauDebugAdapterServerDescriptorFactory implements vscode.DebugAdapterDescriptorFactory {

    createDebugAdapterDescriptor(session: vscode.DebugSession, executable: vscode.DebugAdapterExecutable | undefined): vscode.ProviderResult<vscode.DebugAdapterDescriptor> {
        return new vscode.DebugAdapterServer(session.configuration.port, session.configuration.address);
    }

    dispose() {}
}

class LuauConfigurationProvider implements vscode.DebugConfigurationProvider {

    resolveDebugConfiguration(folder: WorkspaceFolder | undefined, config: DebugConfiguration, token?: CancellationToken): ProviderResult<DebugConfiguration> {
        return config;
    }
}
