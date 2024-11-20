#pragma once

#include <lua.h>

#include "debugger.h"

namespace luau {
namespace debugger {
class Debugger;
}

class Runtime {
 public:
  Runtime();
  ~Runtime();

  void installDebugger(debugger::Debugger* debugger);
  void installLibrary();
  bool runFile(const char* name);

 private:
  lua_State* vm_ = nullptr;
  debugger::Debugger* debugger_ = nullptr;
};

}  // namespace luau