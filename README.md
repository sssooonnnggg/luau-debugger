# Luau debugger

A debugger for Luau with debug adapter protocol(DAP) support.

## Overview

```bash
└── luau_debugger
  |
  ├── debugger    # Debugger implementation, include a DAP server and
  |               # a debugger implemented with luau internal debug
  |               # api directly without hooking
  |
  ├── luaud       # A minimal Luau executable with debug support
  ├── extensions  # VSCode extension for Luau debugger
  └── tests       # Tests with lua scripts
```

## Usage

See [extensions/vscode/README](./extensions/vscode/README.md)

## Dependencies

- luau
- cppdap
  - C++ DAP(Debug Adapter Protocol) library, help to simplify the implementation of a DAP server

## Build
- Clone `cppdap` and `luau` repository to local
- Set `CPP_DAP_ROOT` and `LUAU_ROOT` in `CMakePresets.json`
- Build using CMake Presets by CLI
  - `cmake -S . -B build --preset <configure preset>`
  - `cmake --build --preset <build preset>`
- Or use other IDE such as VSCode or Visual Studio

## Features

- [ ] Debugger features
  - [x] Attach
  - [x] Launch
  - [x] Stop on entry
  - [x] Breakpoints
    - [x] Add break points when running (Considering thread safety)
  - [x] Continue
  - [ ] Force break
  - [x] StackTrace
    - [ ] StackTrace support coroutine
  - [x] Scopes
  - [x] Get variables
    - [x] Locals
    - [x] Upvalues
  - [ ] Display variables
    - [x] nil
    - [x] boolean
    - [x] number
    - [x] string
    - [x] table
      - [x] nested table
      - [x] table with cycle reference
    - [x] vector
    - [x] function
    - [ ] userdata
  - [x] Set variables
  - [x] Repl
  - [x] Watch
  - [x] Hover
  - [x] Single step
    - [x] Step in
    - [x] Step over
    - [x] Step out
  - [x] Disconnect and reconnect
  - [ ] Print in debug console
  - [x] Coroutine

## Notice

- To avoid debug info to be stripped by luau compiler, `Luau::CompileOptions::debugLevel` should be set to `2`