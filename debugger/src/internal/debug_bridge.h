#pragma once

#include <lstate.h>
#include <lua.h>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

#include <dap/protocol.h>
#include <dap/session.h>

#include <internal/breakpoint.h>
#include <internal/file.h>
#include <internal/lua_statics.h>
#include <internal/variable.h>
#include <internal/variable_registry.h>
#include <internal/vm_registry.h>

namespace luau::debugger {

struct BreakContext {
  std::string source_;
  int line_ = 0;
  int depth_ = 0;
  lua_State* L_ = nullptr;
  auto operator<=>(const BreakContext&) const = default;
};

using namespace dap;

class LuaCallbacks;
class DebugBridge final {
 public:
  static DebugBridge* get(lua_State* L);
  DebugBridge(bool stop_on_entry);

  void initialize(lua_State* L);
  bool isDebugBreak();

  // Called from **lua runtime** after lua file is loaded
  // Assume that the top closure from file is already on
  // the stack
  void onLuaFileLoaded(lua_State* L, std::string_view path, bool is_entry);

  // Called from **lua runtime** when debug break encountered
  enum class BreakReason { Step, BreakPoint, Entry };
  void onDebugBreak(lua_State* L, lua_Debug* ar, BreakReason reason);

  // Called from **DAP** when client is ready
  void onConnect(dap::Session* session);

  // Called from **DAP** client when disconnected
  void onDisconnect();

  // Called from **DAP** client when breakpoints changed in file
  void setBreakPoints(std::string_view path,
                      optional<array<SourceBreakpoint>> breakpoints);

  // Called from **DAP** client to resume execution
  void resume();

  // Called from **DAP** client to get current stack trace
  StackTraceResponse getStackTrace();

  // Called from **DAP** client to get scopes for a frame
  void extracted(int& level, lua_State*& L);
  ScopesResponse getScopes(int level);

  // Called from **DAP** client to get variable by variable reference
  VariablesResponse getVariables(int reference);

  // Called from **DAP** client to set variable value
  ResponseOrError<SetVariableResponse> setVariable(
      const SetVariableRequest& request);

  // Called from **DAP** client to step to next line
  void stepOver();

  // Called from **DAP** client to step into function
  void stepIn();

  // Called from **DAP** client to step out of function
  void stepOut();

  // Called from **DAP** client to evaluate expression
  ResponseOrError<EvaluateResponse> evaluate(const EvaluateRequest& request);

  // Thread safe call
  template <class F, class... Args>
  void threadSafeCall(F&& f, Args&&... args) {
    if (isDebugBreak())
      std::invoke(std::forward<F>(f), this, std::forward<Args>(args)...);
    else {
      std::scoped_lock lock(deferred_mutex_);
      deferred_actions_.emplace_back(
          [f = std::forward<F>(f),
           args = std::make_tuple(this, std::forward<Args>(args)...)]() {
            std::apply(f, args);
          });
    }
  }

  void writeDebugConsole(std::string_view msg, lua_State* L, int level = 0);

  VMRegistry& vms() { return vm_registry_; }

 private:
  void initializeCallbacks(lua_State* L);
  void captureOutput(lua_State* L);

  std::string normalizePath(std::string_view path) const;
  bool isBreakOnEntry(lua_State* L) const;

  BreakContext getBreakContext(lua_State* L) const;
  int getStackDepth(lua_State* L) const;

  std::string stopReasonToString(BreakReason reason) const;

  void interruptUpdate();

  void clearBreakPoints();

  // Return true if execution should be stopped, return false if execution
  // should continue
  using SingleStepProcessor = std::function<bool(lua_State*, lua_Debug* ar)>;
  void processSingleStep(SingleStepProcessor processor);
  void enableDebugStep(bool enable);

  void resumeInternal();

  ResponseOrError<EvaluateResponse> evaluateRepl(
      const EvaluateRequest& request);
  ResponseOrError<EvaluateResponse> evaluateWatch(
      const EvaluateRequest& request);
  ResponseOrError<EvaluateResponse> evaluateHover(
      const EvaluateRequest& request);

  ResponseOrError<EvaluateResponse> evalWithEnv(const EvaluateRequest& request);

  BreakPoint* findBreakPoint(lua_State* L);

 private:
  friend class LuaStatics;

  VMRegistry vm_registry_;

  bool stop_on_entry_ = false;
  std::string entry_path_;

  std::unordered_map<std::string, File> files_;
  lua_State* break_vm_ = nullptr;

  std::mutex break_mutex_;
  bool resume_ = false;
  std::condition_variable resume_cv_;

  dap::Session* session_ = nullptr;
  std::condition_variable session_cv_;

  VariableRegistry variable_registry_;

  std::vector<std::function<void()>> deferred_actions_;
  std::mutex deferred_mutex_;

  SingleStepProcessor single_step_processor_ = nullptr;
};
}  // namespace luau::debugger