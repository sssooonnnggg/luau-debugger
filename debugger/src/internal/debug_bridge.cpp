#include <algorithm>
#include <filesystem>
#include <format>
#include <mutex>
#include <optional>
#include <unordered_map>

#include <lstate.h>
#include <lua.h>
#include <lualib.h>

#include <dap/protocol.h>
#include <dap/session.h>
#include <dap/types.h>

#include <internal/breakpoint.h>
#include <internal/debug_bridge.h>
#include <internal/file.h>
#include <internal/log.h>
#include <internal/lua_statics.h>
#include <internal/utils.h>
#include <internal/variable.h>

#include "debugger.h"

namespace luau::debugger {

DebugBridge::DebugBridge(bool stop_on_entry) : stop_on_entry_(stop_on_entry) {}

DebugBridge* DebugBridge::get(lua_State* L) {
  lua_State* main_vm = lua_mainthread(L);
  auto* debugger = reinterpret_cast<Debugger*>(lua_getthreaddata(main_vm));
  if (debugger == nullptr)
    return nullptr;
  return debugger->debug_bridge_.get();
}

void DebugBridge::initialize(lua_State* L) {
  vm_registry_.registerVM(L);

  initializeCallbacks(L);
  captureOutput(L);

  lua_singlestep(L, true);
}

void DebugBridge::initializeCallbacks(lua_State* L) {
  lua_Callbacks* cb = lua_callbacks(L);
  cb->debugbreak = LuaStatics::debugbreak;
  cb->interrupt = LuaStatics::interrupt;
  cb->userthread = LuaStatics::userthread;
}

void DebugBridge::captureOutput(lua_State* L) {
  const char* global_print = "print";
  auto enable = lua_getreadonly(L, LUA_GLOBALSINDEX);
  lua_setreadonly(L, LUA_GLOBALSINDEX, false);
  lua_getglobal(L, global_print);
  if (lua_isfunction(L, -1)) {
    lua_pushcclosure(L, LuaStatics::print, "debugprint", 1);
    lua_setglobal(L, global_print);
  }
  lua_setreadonly(L, LUA_GLOBALSINDEX, enable);
}

bool DebugBridge::isDebugBreak() {
  std::scoped_lock lock(break_mutex_);
  return !resume_;
}

void DebugBridge::onDebugBreak(lua_State* L,
                               lua_Debug* ar,
                               BreakReason reason) {
  std::unique_lock<std::mutex> lock(break_mutex_);

  dap::StoppedEvent event{.reason = stopReasonToString(reason)};
  event.threadId = 1;
  if (reason == BreakReason::Entry) {
    if (session_ == nullptr) {
      DEBUGGER_LOG_INFO(
          "Session is not initialized, wait for client connection");
      session_cv_.wait(lock, [this] { return session_ != nullptr; });
    }
  } else if (session_ == nullptr) {
    DEBUGGER_LOG_INFO("Session is lost, ignore breakpoints");
    return;
  }
  session_->send(event);

  break_vm_ = L;
  resume_ = false;
  resume_cv_.wait(lock, [this] { return resume_; });
  break_vm_ = nullptr;
}

std::string DebugBridge::stopReasonToString(BreakReason reason) const {
  switch (reason) {
    case BreakReason::Entry:
      return "entry";
    case BreakReason::BreakPoint:
      return "breakpoint";
    case BreakReason::Step:
      return "step";
    default:
      return "breakpoint";
  }
}

StackTraceResponse DebugBridge::getStackTrace() {
  if (!isDebugBreak())
    return StackTraceResponse{};

  StackTraceResponse response;
  lua_State* L = break_vm_;

  variable_registry_.update(break_vm_);

  lua_Debug ar;
  for (int level = 0; lua_getinfo(L, level, "sln", &ar); ++level) {
    StackFrame frame;
    frame.id = level;
    frame.name = ar.name ? ar.name : "unknown";
    frame.source = Source{};
    if (ar.source)
      frame.source->path = normalizePath(ar.source);
    frame.line = ar.currentline;
    response.stackFrames.emplace_back(std::move(frame));
  }

  // TODO: consider coroutine stack

  response.totalFrames = response.stackFrames.size();
  return response;
}

ScopesResponse DebugBridge::getScopes(int level) {
  if (!isDebugBreak())
    return {};

  ScopesResponse response;

  response.scopes = {
      dap::Scope{.expensive = false,
                 .name = "Local",
                 .variablesReference =
                     variable_registry_.getLocalScope(level).getKey()},
      dap::Scope{.expensive = false,
                 .name = "Upvalues",
                 .variablesReference =
                     variable_registry_.getUpvalueScope(level).getKey()}};
  return response;
}

VariablesResponse DebugBridge::getVariables(int reference) {
  if (!isDebugBreak())
    return {};

  auto variables = variable_registry_.getVariables(Scope(reference));
  if (variables == nullptr)
    return VariablesResponse{};

  VariablesResponse response;
  for (const auto& variable : (*variables))
    response.variables.emplace_back(
        dap::Variable{.name = std::string(variable.getName()),
                      .value = std::string(variable.getValue()),
                      .variablesReference = variable.getScope().getKey()});
  return response;
}

ResponseOrError<SetVariableResponse> DebugBridge::setVariable(
    const SetVariableRequest& request) {
  if (!isDebugBreak())
    return {};

  auto result = variable_registry_.getVariables(request.variablesReference);
  if (result == nullptr)
    return Error{"Variable scope not found"};

  auto& scope = result->first;
  auto& variables = result->second;
  auto it = std::find_if(variables.begin(), variables.end(),
                         [&request](const auto& variable) {
                           return variable.getName() == request.name;
                         });
  if (it == variables.end())
    return Error{"Variable not found"};

  std::string new_value;
  try {
    new_value = it->setValue(scope, request.value);
  } catch (const std::exception& e) {
    return Error{e.what()};
  }

  return SetVariableResponse{.value = new_value,
                             .variablesReference = it->getScope().getKey()};
}

void DebugBridge::resume() {
  DEBUGGER_ASSERT(isDebugBreak());

  // Disable single step
  processSingleStep(nullptr);
  resumeInternal();
}

void DebugBridge::onLuaFileLoaded(lua_State* L,
                                  std::string_view path,
                                  bool is_entry) {
  auto normalized_path = normalizePath(path);

  auto it = files_.find(normalized_path);
  if (it == files_.end()) {
    File file;
    file.setPath(normalized_path);
    file.addRef(FileRef(L));

    DEBUGGER_LOG_INFO("[onLuaFileLoaded] New file loaded: {}", normalized_path);
    it = files_.emplace(normalized_path, std::move(file)).first;
  } else {
    DEBUGGER_LOG_INFO(
        "[onLuaFileLoaded] File already loaded, replace with new: {}",
        normalized_path);

    it->second.addRef(FileRef(L));
  }

  if (is_entry && stop_on_entry_) {
    it->second.addBreakPoint(1);
    entry_path_ = normalized_path;
  }
}

void DebugBridge::setBreakPoints(
    std::string_view path,
    optional<array<SourceBreakpoint>> breakpoints) {
  auto normalized_path = normalizePath(path);

  // Clear all breakpoints
  if (!breakpoints.has_value()) {
    DEBUGGER_LOG_INFO("[setBreakPoint] clear all breakpoints: {}",
                      normalized_path);
    auto it = files_.find(normalized_path);
    if (it == files_.end())
      return;
    it->second.clearBreakPoints();
    files_.erase(it);
  }

  auto it = files_.find(normalized_path);
  std::unordered_map<int, BreakPoint> bps;
  for (const auto& bp : *breakpoints)
    bps.emplace(static_cast<int>(bp.line), BreakPoint::create(bp.line));

  if (it == files_.end()) {
    DEBUGGER_LOG_INFO("[setBreakPoint] create new file with breakpoints: {}",
                      normalized_path);

    File file;
    file.setPath(normalized_path);
    it = files_.emplace(normalized_path, File()).first;
  } else
    DEBUGGER_LOG_INFO("[setBreakPoint] file already loaded: {}",
                      normalized_path);

  // Update existing file with breakpoints
  auto& file = it->second;
  file.setBreakPoints(bps);
}

std::string DebugBridge::normalizePath(std::string_view path) const {
  if (path.empty())
    return std::string{};
  std::string_view prefix_removed =
      (path[0] == '@' || path[0] == '=') ? path.substr(1) : path;
  return std::filesystem::weakly_canonical(prefix_removed).string();
}

BreakContext DebugBridge::getBreakContext(lua_State* L) const {
  lua_Debug ar;
  lua_getinfo(L, 0, "sl", &ar);
  return BreakContext{
      .source_ = normalizePath(ar.source),
      .line_ = ar.currentline,
      .depth_ = getStackDepth(L),
      .L_ = L,
  };
}

int DebugBridge::getStackDepth(lua_State* L) const {
  int depth = lua_stackdepth(L);
  auto* parent = vm_registry_.getParent(L);
  while (parent != nullptr) {
    depth += lua_stackdepth(parent);
    parent = vm_registry_.getParent(parent);
  }
  return depth;
}

bool DebugBridge::isBreakOnEntry(lua_State* L) const {
  auto context = getBreakContext(L);
  return entry_path_ == context.source_ && context.line_ == 1;
}

void DebugBridge::interruptUpdate() {
  std::vector<std::function<void()>> actions;
  {
    std::scoped_lock lock(deferred_mutex_);
    std::swap(deferred_actions_, actions);
  }

  for (const auto& action : actions)
    action();
}

void DebugBridge::clearBreakPoints() {
  for (auto& [_, file] : files_)
    file.clearBreakPoints();
}

void DebugBridge::onConnect(dap::Session* session) {
  std::scoped_lock lock(break_mutex_);
  session_ = session;
  session_cv_.notify_one();
}

void DebugBridge::onDisconnect() {
  threadSafeCall(&DebugBridge::clearBreakPoints);
  std::scoped_lock lock(break_mutex_);
  if (!resume_) {
    resume_ = true;
    resume_cv_.notify_one();
  }
  session_ = nullptr;
}

void DebugBridge::stepIn() {
  if (!isDebugBreak())
    return;

  auto old_ctx = getBreakContext(break_vm_);
  processSingleStep([this, old_ctx](lua_State* L, lua_Debug* ar) -> bool {
    return old_ctx != getBreakContext(L);
  });
  resumeInternal();
}

void DebugBridge::stepOut() {
  if (!isDebugBreak())
    return;

  auto old_ctx = getBreakContext(break_vm_);
  processSingleStep([this, old_ctx](lua_State* L, lua_Debug* ar) -> bool {
    auto ctx = getBreakContext(L);
    return ctx.depth_ < old_ctx.depth_;
  });
  resumeInternal();
}

void DebugBridge::stepOver() {
  if (!isDebugBreak())
    return;

  auto old_ctx = getBreakContext(break_vm_);
  processSingleStep([this, old_ctx](lua_State* L, lua_Debug* ar) -> bool {
    // Step over yield boundary
    if (vm_registry_.isAlive(old_ctx.L_) && old_ctx.L_->status == LUA_YIELD)
      return false;

    auto ctx = getBreakContext(L);

    if (L != old_ctx.L_ && !vm_registry_.isChild(old_ctx.L_, L))
      return false;

    // Normal step over
    return (ctx.depth_ == old_ctx.depth_ && ctx.line_ != old_ctx.line_) ||
           ctx.depth_ < old_ctx.depth_;
  });
  resumeInternal();
}

void DebugBridge::processSingleStep(SingleStepProcessor processor) {
  enableDebugStep(processor != nullptr);
  single_step_processor_ = processor;
}

void DebugBridge::enableDebugStep(bool enable) {
  lua_State* L = vm_registry_.getRoot(break_vm_);
  auto callbacks = lua_callbacks(L);
  if (!enable) {
    callbacks->debugstep = nullptr;
    return;
  }

  callbacks->debugstep = LuaStatics::debugstep;
}

void DebugBridge::resumeInternal() {
  std::unique_lock<std::mutex> lock(break_mutex_);
  DEBUGGER_LOG_INFO("[resume] Resume execution");
  resume_ = true;
  resume_cv_.notify_one();
}

ResponseOrError<EvaluateResponse> DebugBridge::evaluate(
    const EvaluateRequest& request) {
  if (!isDebugBreak()) {
    // TODO: support evaluate when not in debug break
    return Error{"Evaluate request is not allowed when not in debug break"};
  }

  if (!request.context.has_value())
    return Error{"Evaluate request must have context"};

  auto context = request.context.value();
  DEBUGGER_LOG_INFO("[evaluate] Evaluate context: {}", context);

  if (context == "repl")
    return evaluateRepl(request);
  else if (context == "watch")
    return evaluateWatch(request);
  else {
    DEBUGGER_LOG_ERROR("[evaluate] Invalid evaluate context: {}", context);
    return Error{"Invalid evaluate context"};
  }
}

ResponseOrError<EvaluateResponse> DebugBridge::evaluateRepl(
    const EvaluateRequest& request) {
  return evalWithEnv(request);
}

ResponseOrError<EvaluateResponse> DebugBridge::evaluateWatch(
    const EvaluateRequest& request) {
  return evalWithEnv(request);
}

ResponseOrError<EvaluateResponse> DebugBridge::evaluateHover(
    const EvaluateRequest& request) {
  return evalWithEnv(request);
}

ResponseOrError<EvaluateResponse> DebugBridge::evalWithEnv(
    const EvaluateRequest& request) {
  int level = 0;
  if (request.frameId.has_value())
    level = request.frameId.value();

  if (!lua_utils::pushBreakEnv(break_vm_, level))
    return Error{"Failed to push break environment"};

  int ret = lua_utils::eval(break_vm_, request.expression, -1).value_or(1);
  std::string result;
  for (int i = ret; i >= 1; --i) {
    result += lua_utils::toString(break_vm_, -i);
    if (i != 1)
      result += "\n";
  }

  // Pop result
  if (ret > 0)
    lua_pop(break_vm_, ret);

  // Pop the environment
  lua_pop(break_vm_, 1);

  EvaluateResponse response{.result = result};
  return response;
}

void DebugBridge::writeDebugConsole(std::string_view output,
                                    lua_State* L,
                                    int level) {
  if (session_ == nullptr)
    return;

  dap::OutputEvent event;
  event.output = std::string{output};

  lua_Debug ar;
  if (L != nullptr && lua_getinfo(L, 1, "sln", &ar)) {
    event.line = ar.currentline;
    event.source = dap::Source{.path = normalizePath(ar.source)};
  }

  session_->send(std::move(event));
}

}  // namespace luau::debugger