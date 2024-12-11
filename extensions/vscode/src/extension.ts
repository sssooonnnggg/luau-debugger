//
// Just a thin wrapper for the luau_debugger which is implemented in C++.
//

import * as vscode from 'vscode';
import * as path from 'path';
import * as fs from 'fs';
import * as child_process from 'child_process';
import * as readline from 'readline';
import { DebugProtocol } from "@vscode/debugprotocol";
import { WorkspaceFolder, DebugConfiguration, ProviderResult, CancellationToken } from 'vscode';

const LUAU_DEBUG_TYPE = 'luau';
const LUAUD = 'luaud.exe';

let output: vscode.OutputChannel;
let source_map = new Map<string, string>();
let reverse_source_map = new Map<string, string>();

export function activate(context: vscode.ExtensionContext) {
  const provider = new LuauConfigurationProvider();
  output = vscode.window.createOutputChannel('luau-debugger');
  output.show();
  context.subscriptions.push(vscode.debug.registerDebugConfigurationProvider(LUAU_DEBUG_TYPE, provider));
  context.subscriptions.push(vscode.debug.registerDebugAdapterDescriptorFactory(LUAU_DEBUG_TYPE, new LuauDebugAdapterServerDescriptorFactory()));
  context.subscriptions.push(vscode.debug.registerDebugAdapterTrackerFactory(LUAU_DEBUG_TYPE, new LuauDebugAdapterTrackerFactory()));
}

export function deactivate() { }

function killDebugger() {
  try { child_process.execSync(`taskkill /f /im ${LUAUD}`) } catch (e) { }
}

async function spawnLuauDebugger(session: vscode.DebugSession): Promise<child_process.ChildProcessWithoutNullStreams | null> {

  return new Promise((resolve, reject) => {
    const extension_folder = vscode.extensions.getExtension('sssooonnnggg.luau-debugger')!.extensionPath;
    const debugger_path = extension_folder + `/debugger/${LUAUD}`;
    if (vscode.workspace.workspaceFolders?.length == 0) {
      vscode.window.showErrorMessage('No workspace folder is opened.');
      return;
    }
    const workspaces = vscode.workspace.workspaceFolders!;
    const entry_path = path.resolve(workspaces[0].uri.fsPath, session.configuration.program);
    if (!fs.existsSync(entry_path)) {
      vscode.window.showErrorMessage(`lua file "${entry_path}" does not exist.`);
      return;
    }

    killDebugger();
    const process = child_process.spawn(debugger_path, [session.configuration.port, entry_path], { detached: true, stdio: 'pipe' });

    let resolved = false;

    let timeout = setTimeout(() => {
      killDebugger();
      vscode.window.showErrorMessage('Failed to start luau debugger, timeout.');
      resolve(null);
    }, 1000 * 60);

    const stdout = readline.createInterface(process.stdout);
    stdout.on('line', line => {
      const waiting = 'wait for client connection'
      output.appendLine(line);
      if (line.includes(waiting)) {
        clearTimeout(timeout);
        resolve(process);
      }
    });

    const stderr = readline.createInterface(process.stderr);
    stderr.on('line', line => {
      output.appendLine(line);
    });

    process.on('exit', code => {
      console.log(`luau debugger exited with code ${code}`);
      resolve(null);
    });
  });
}

class LuauDebugAdapterServerDescriptorFactory implements vscode.DebugAdapterDescriptorFactory {

  process: child_process.ChildProcessWithoutNullStreams | null = null;

  async createDebugAdapterDescriptor(session: vscode.DebugSession, executable: vscode.DebugAdapterExecutable | undefined): Promise<vscode.DebugAdapterDescriptor> {
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

class LuauConfigurationProvider implements vscode.DebugConfigurationProvider {

  resolveDebugConfiguration(folder: WorkspaceFolder | undefined, config: DebugConfiguration, token?: CancellationToken): ProviderResult<DebugConfiguration> {
    return config;
  }

  resolveDebugConfigurationWithSubstitutedVariables(folder: WorkspaceFolder | undefined, config: DebugConfiguration, token?: CancellationToken): ProviderResult<DebugConfiguration> {
    this.initSourceMap(config);
    return config;
  }

  initSourceMap(config: DebugConfiguration) {
    source_map.clear();
    reverse_source_map.clear();
    let input = config.sourceMap;
    if (input != undefined && typeof input == 'object') {
      for (let key in input) {
        let value = input[key];
        source_map.set(key, value);
        reverse_source_map.set(value, key);
      }
    }
  }
}

class LuauDebugAdapterTrackerFactory implements vscode.DebugAdapterTrackerFactory {

  createDebugAdapterTracker(session: vscode.DebugSession): vscode.ProviderResult<vscode.DebugAdapterTracker> {
    return {
      onWillReceiveMessage: (msg) => {
        // reverse mapping breakpoints
        if (msg.type == "request" && msg.command == "setBreakpoints") {
          let request = msg as DebugProtocol.SetBreakpointsRequest;
          let source = request.arguments.source;
          if (source && source.path)
            this.remappingSource(source, reverse_source_map);
        }
        // output.appendLine(`-> ${JSON.stringify(msg)}`);
      },
      onDidSendMessage: (msg) => {
        // remapping stack trace
        if (msg.type == "response" && msg.success && msg.command == "stackTrace") {
          let response = msg as DebugProtocol.StackTraceResponse;
          for (let frame of response.body.stackFrames) {
            let source = frame.source;
            if (source && source.path)
              this.remappingSource(source, source_map);
          }
        }
        else if (msg.type == "response" && msg.success && msg.command == "variables") {
          let response = msg as DebugProtocol.VariablesResponse;
          // Sort variables by name
          response.body.variables.sort((a, b) => {
            return a.name.localeCompare(b.name);
          });
        }
        // output.appendLine(`<- ${JSON.stringify(msg)}`);
      }


    }
  }

  private remappingSource(source: DebugProtocol.Source | undefined, source_map: Map<string, string>) {
    if (source && source.path) {
      let input_path = source.path;
      input_path = input_path;
      for (let [path, mapped] of source_map) {
        for (let p of [path[0].toLowerCase() + path.slice(1), path[0].toUpperCase() + path.slice(1)]) {
          if (input_path.startsWith(p)) {
            let mapped_path = input_path.replace(p, mapped);
            const is_posix = mapped.includes('/');
            source.path = is_posix ? mapped_path.replace(/\\/g, '/') : mapped_path.replace(/\//g, '\\');
            break;
          }
        }
      }
    }
  }
}
