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
  bool runFile(const char* name);

  void setErrorHandler(std::function<void(std::string_view)> handler) {
    errorHandler_ = handler;
  }

 private:
  static void error(std::string_view msg) {
    if (errorHandler_)
      errorHandler_(std::format("\x1B[31m{}\033[0m\n", msg));
  }

 private:
  lua_State* vm_ = nullptr;
  debugger::Debugger* debugger_ = nullptr;
  inline static std::function<void(std::string_view)> errorHandler_ = nullptr;
};

}  // namespace luau