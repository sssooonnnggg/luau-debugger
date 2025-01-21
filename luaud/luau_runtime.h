#pragma once

#include <lua.h>
#include <format>

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
  void reset();
  bool runFile(const char* name);

  void setErrorHandler(std::function<void(std::string_view)> handler);
  void onError(std::string_view msg, lua_State* L);

 private:
  lua_State* vm_ = nullptr;
  debugger::Debugger* debugger_ = nullptr;
  std::function<void(std::string_view)> errorHandler_ = nullptr;
};

}  // namespace luau