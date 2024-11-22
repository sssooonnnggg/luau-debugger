#include <algorithm>
#include <filesystem>
#include <format>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include <lstate.h>
#include <lua.h>

#include <dap/protocol.h>

#include <dap/session.h>
#include <internal/breakpoint.h>
#include <internal/debug_bridge.h>
#include <internal/log.h>
#include <internal/utils.h>
#include <internal/variable.h>

#include <debugger.h>

namespace luau::debugger {

DebugBridge::DebugBridge(bool stop_on_entry) : stop_on_entry_(stop_on_entry) {}

DebugBridge* DebugBridge::getDebugBridge(lua_State* L) {
  lua_State* main_vm = lua_mainthread(L);
  return reinterpret_cast<Debugger*>(lua_getthreaddata(main_vm))
      ->debug_bridge_.get();
}

void DebugBridge::initialize(lua_State* L) {
  main_vm_ = L;
  lua_Callbacks* cb = lua_callbacks(L);
  cb->debugbreak = [](lua_State* L, lua_Debug* ar) {
    auto bridge = DebugBridge::getDebugBridge(L);
    bridge->onDebugBreak(L, ar,
                         bridge->isBreakOnEntry(L) ? BreakReason::Entry
                                                   : BreakReason::BreakPoint);
  };

  cb->interrupt = [](lua_State* L, int gc) {
    auto bridge = DebugBridge::getDebugBridge(L);
    bridge->processPendingMainThreadActions();
  };

  lua_singlestep(L, true);
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
    if (auto file = files_.find(entry_path_); file != files_.end())
      file->second.removeBreakPoint(1);
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
  StackTraceResponse response;
  std::scoped_lock lock(break_mutex_);
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

  response.totalFrames = response.stackFrames.size();
  return response;
}

ScopesResponse DebugBridge::getScopes(int level) {
  std::scoped_lock lock(break_mutex_);
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
  std::scoped_lock lock(break_mutex_);
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
  std::scoped_lock lock(break_mutex_);
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
  auto file = File::fromLuaState(L, normalized_path);

  if (is_entry && stop_on_entry_) {
    file.addBreakPoint(1);
    entry_path_ = normalized_path;
  }

  auto it = files_.find(normalized_path);
  if (it == files_.end()) {
    DEBUGGER_LOG_INFO("[onLuaFileLoaded] New file loaded: {}", normalized_path);
    files_.emplace(normalized_path, std::move(file));
  } else {
    DEBUGGER_LOG_INFO(
        "[onLuaFileLoaded] File already loaded, replace with new: {}",
        normalized_path);

    auto& old_file = it->second;
    file.syncBreakpoints(old_file);

    it->second = std::move(file);
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

  // New file with breakpoints
  if (it == files_.end()) {
    DEBUGGER_LOG_INFO("[setBreakPoint] create new file with breakpoints: {}",
                      normalized_path);
    std::unordered_map<int, BreakPoint> bps;
    for (const auto& bp : *breakpoints)
      bps.emplace(bp.line, BreakPoint::create(bp.line));

    files_.emplace(normalized_path, File::fromBreakPoints(path, bps));
    return;
  }

  // Update existing file with breakpoints
  std::unordered_set<int> settled;
  auto& file = it->second;

  DEBUGGER_LOG_INFO("[setBreakPoint] file loaded and registered: {}",
                    normalized_path);
  for (const auto& bp : *breakpoints) {
    file.addBreakPoint(bp.line);
    settled.insert(bp.line);
  }
  file.removeBreakPointsIf([&settled](const BreakPoint& bp) {
    return settled.find(bp.line()) == settled.end();
  });
}

std::string DebugBridge::normalizePath(std::string_view path) const {
  if (path.empty())
    return std::string{};
  std::string_view prefix_removed =
      (path[0] == '@' || path[0] == '=') ? path.substr(1) : path;
  return std::filesystem::weakly_canonical(prefix_removed).string();
}

BreakContext DebugBridge::getBreakContext(lua_State* L) const {
  int depth = lua_stackdepth(L);
  lua_Debug ar;
  lua_getinfo(L, 0, "sl", &ar);
  return BreakContext{
      .source_ = normalizePath(ar.source),
      .line_ = ar.currentline,
      .depth_ = depth,
      .L_ = L,
  };
}

bool DebugBridge::isBreakOnEntry(lua_State* L) const {
  auto context = getBreakContext(L);
  return entry_path_ == context.source_ && context.line_ == 1;
}

void DebugBridge::processPendingMainThreadActions() {
  std::vector<std::function<void()>> actions;
  {
    std::scoped_lock lock(main_thread_action_mutex_);
    std::swap(main_thread_actions_, actions);
  }

  for (const auto& action : actions)
    action();
}

void DebugBridge::removeAllBreakPoints() {
  for (auto& [_, file] : files_)
    file.clearBreakPoints();
}

void DebugBridge::onConnect(dap::Session* session) {
  std::scoped_lock lock(break_mutex_);
  session_ = session;
  session_cv_.notify_one();
}

void DebugBridge::onDisconnect() {
  std::scoped_lock lock(break_mutex_);
  if (!resume_) {
    resume_ = true;
    resume_cv_.notify_one();
  }
  removeAllBreakPoints();
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
    if (old_ctx.L_->status == LUA_YIELD)
      return false;
    auto ctx = getBreakContext(L);
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
  auto callbacks = lua_callbacks(main_vm_);
  if (!enable) {
    callbacks->debugstep = nullptr;
    return;
  }

  callbacks->debugstep = [](lua_State* L, lua_Debug* ar) {
    auto bridge = DebugBridge::getDebugBridge(L);
    if (bridge->single_step_processor_ != nullptr)
      if ((bridge->single_step_processor_)(L, ar))
        bridge->onDebugBreak(L, ar, BreakReason::Step);
  };
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

}  // namespace luau::debugger