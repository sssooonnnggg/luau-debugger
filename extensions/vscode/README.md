# Luau debugger

A debugger for Luau with debug adapter protocol(DAP) support.

![](docs/demo.gif)

## Usage

- Install `luau-debugger` [extension](https://marketplace.visualstudio.com/items?itemName=sssooonnnggg.luau-debugger)
- There are three ways to use this extension:
  - **Launch**: Directly run the debugger installed with the extension.
  - **Attach**: Attach to the luau runtime running externally.
  - **Advance**: Integrate `luau-debugger` into your own project.

### Launch
- Add a launch configuration in `launch.json`
  ```json
  {
    "configurations": [
      {
        "type": "luau",
        "request": "launch",
        "name": "launch luau debugger",
        "program": "${workspaceFolder}/main.lua",
        "port": 58000
      },
    ]
  }
  ```
  - `program`: The path to the lua script you want to debug
  - `port`: The port number to communicate with the debugger
- Press `F5` to start debugging, enjoy!

### Attach
- Get a `luaud` executable from [release](https://github.com/sssooonnnggg/luau-debugger/releases) which a luau runtime with debug support
- Open lua folders in VSCode
- Add a launch configuration in `launch.json`
  ```json
  {
    "configurations": [
      {
        "type": "luau",
        "request": "attach",
        "name": "attach to luau debugger",
        "address": "localhost",
        "port": 58000
      }
    ]
  }
  ```
- Using `luaud [PORT] [LUA_ENTRY]` to execute lua script with debug support:
  ```bash
  luaud 58000 D:/my_lua_projects/hello_world.lua
  ```
- Press `F5` to start debugging, enjoy!

### Integrate with luau-debugger in your project
- [Build luau-debugger from source](https://github.com/sssooonnnggg/luau-debugger#build)
- Use the interface provided by `luau-debugger` library.
  - You can refer to the implementation of `luaud` which is a minimal luau runtime integrated with `luau-debugger`.
- Run your project and debug the luau code using a configuration similar to `Attach`.

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
    - [ ] StackTrace support chaining coroutine
    - [ ] Support switching stacktrace between different coroutines
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
  - [x] Print to debug console
  - [x] Coroutine
