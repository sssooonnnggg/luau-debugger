# Luau debugger

A debugger for Luau with debug adapter protocol(DAP) support.

## Usage
- Get a `luaud` executable
  - Download from [release](https://github.com/sssooonnnggg/luau-debugger/releases/download/v1.0.14/luaud.zip)
  - Or build from [source](https://github.com/sssooonnnggg/luau-debugger)
- Install `luau-debugger` [extension](https://marketplace.visualstudio.com/items?itemName=sssooonnnggg.luau-debugger)
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
- Using `luaud` to execute lua script with debug support, the first argument is the port number, the second argument is the path to the lua script you want to debug
  ```bash
  luaud 58000 D:/my_lua_projects/hello_world.lua
  ```
- Press `F5` to start debugging, enjoy!

  ![](docs/demo.gif)

## Features

- [ ] Debugger features
  - [x] Attach
  - [ ] Launch
  - [x] Stop on entry
  - [x] Breakpoints
    - [x] Add break points when running (Considering thread safety)
  - [x] Continue
  - [ ] Force break
  - [x] StackTrace
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
  - [ ] Set variables
  - [x] Repl
  - [x] Watch
  - [x] Hover
  - [x] Single step
    - [x] Step in
    - [x] Step over
    - [x] Step out
  - [x] Disconnect and reconnect
  - [ ] Log
  - [ ] Coroutine

## Notice

- To avoid debug info to be stripped by luau compiler, `Luau::CompileOptions::debugLevel` should be set to `2`