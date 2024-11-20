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
  ├── luaud       # A Luau executable with debugger support, for testing purpose
  ├── extensions  # VSCode extension for Luau debugger
  └── tests       # Tests with lua scripts
```

## Dependencies

- luau
- cppdap
  - C++ DAP(Debug Adapter Protocol) library, help to simplify the implementation of a DAP server

## Build
- Clone `cppdap` and `luau` repository to local
- Set `CPP_DAP_ROOT` and `LUAU_ROOT` in `CMakePresets.json`
- Build using CMake Presets by CLI
  - `cmake -S . -B build --preset Windows-clang-debug`
  - `cmake --build --preset DebugBuild`
- Or use other IDE such as VSCode or Visual Studio

## TODO

- [ ] Debugger features
  - [x] Stop on entry
  - [x] Breakpoints
    - [x] Add break points when running (Considering thread safety)
  - [x] Continue
  - [ ] Force break
  - [x] StackTrace
  - [x] Scopes
  - [x] Get Variables
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
  - [ ] Set Variable
  - [ ] Evaluate
  - [ ] Watch
  - [ ] Single Step
    - [ ] StepIn
    - [ ] StepOver
    - [ ] StepOut
  - [ ] Disconnect and reconnect
  - [ ] Log
  - [ ] Coroutine

## Notice

- To avoid debug info to be stripped by luau compiler, `Luau::CompileOptions::debugLevel` should be set to `2`