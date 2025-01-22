# Luau debugger
![Build status](https://github.com/sssooonnnggg/luau-debugger/actions/workflows/build.yml/badge.svg?branch=master)

A debugger for Luau with debug adapter protocol(DAP) support.

![](docs/demo.gif)

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
  └── tests       # Test lua scripts
```

## Usage

See [extensions/vscode/README](./extensions/vscode/README.md)

## Dependencies

- [luau](https://github.com/luau-lang/luau)
- [cppdap](https://github.com/google/cppdap)

## Build
- Clone `cppdap` to local
  - Inside `cppdap` root, run `git submodule update --init`
- Clone `luau` to local
- Build using CMake Presets with CLI or preset, for example with CLI:
  - `cmake -DLUAU_ROOT=<luau path> -DCPP_DAP_ROOT=<cppdap path> -S . -B build`
  - `cmake --build`

## Features

See [features](./extensions/vscode/README.md#features)

## Integration with luau-debugger

See [luaud](./luaud/main.cpp) for how to integrate with `luau-debugger` in your project.

Tips:
- To avoid debug info to be stripped by luau compiler, `Luau::CompileOptions::debugLevel` should be set to `2`
- Call `Debugger::initialize(lua_State* L)` to initialize the debugger
  - `Debugger::initialize(lua_State* L)` can be called multiple times to debug multiple lua states
  - `Debugger::release(lua_State* L)` can be called to release the lua state before calling `lua_close`
- Call `Debugger::onLuaFileLoaded(lua_State* L, std::string_view path, bool is_entry)` when lua file entry is loaded and lua files are required
- Call `Debugger::listen()` to start the DAP server
- Call `Debugger::onError(std::string_view msg, lua_State* L)` if you want to redirect Lua error messages to the debug console.

### Displaying `userdata` Variables

To display `userdata` variables in the debugger:
- Implement the `__iter` metamethod for `userdata` as described [here](https://github.com/luau-lang/rfcs/blob/master/docs/generalized-iteration.md).
- Alternatively, implement a `__getters` metamethod that returns a table where the keys are property names and the values are functions that return the property values.
