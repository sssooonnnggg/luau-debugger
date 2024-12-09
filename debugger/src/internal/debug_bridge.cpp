#include <algorithm>
#include <filesystem>
#include <format>
#include <mutex>
#include <optional>
#include <thread>
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
#include <internal/scope.h>
#include <internal/utils.h>
#include <internal/utils/lua_types.h>
#include <internal/variable.h>

#include "debugger.h"

namespace luau::debugger {

DebugBridge::DebugBridge(bool stop_on_entry)
    : stop_on_entry_(stop_on_entry), task_pool_(std::this_thread::get_id()) {}

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

void DebugBridge::setRoot(std::string_view root) {
  lua_root_ = std::string(root);
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

  if (reason == BreakReason::BreakPoint && !hitBreakPoint(L))
    return;

  session_->send(event);

  mainThreadWait(L, lock);
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
  lua_utils::DisableDebugStep _(break_vm_);

  refetchVariables();
  auto frames = refetchStackFrames();

  response.stackFrames = frames;
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
                     variable_registry_.getUpvalueScope(level).getKey()},
      dap::Scope{
          .expensive = false,
          .name = "Globals",
          .variablesReference = variable_registry_.getGlobalScope().getKey()},
  };
  return response;
}

VariablesResponse DebugBridge::getVariables(int reference) {
  if (!isDebugBreak())
    return {};

  std::vector<Variable>* variables;
  executeInMainThread([&]() {
    variables = variable_registry_.getVariables(Scope(reference), true);
  });
  if (variables == nullptr)
    return VariablesResponse{};

  VariablesResponse response;
  for (const auto& variable : (*variables))
    response.variables.emplace_back(
        dap::Variable{.name = std::string(variable.getName()),
                      .type = variable.getType(),
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

  ResponseOrError<SetVariableResponse> response;
  executeInMainThread([&]() {
    std::string new_value;
    try {
      new_value = it->setValue(scope, request.value);
    } catch (const std::exception& e) {
      response = Error{e.what()};
      return;
    }
    response = SetVariableResponse{
        .value = new_value, .variablesReference = it->getScope().getKey()};
  });

  return response;
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
    file.addRef(LuaFileRef(L));

    DEBUGGER_LOG_INFO("[onLuaFileLoaded] New file loaded: {}", normalized_path);
    it = files_.emplace(normalized_path, std::move(file)).first;
  } else {
    DEBUGGER_LOG_INFO(
        "[onLuaFileLoaded] File already loaded, replace with new: {}",
        normalized_path);

    it->second.addRef(LuaFileRef(L));
  }

  if (is_entry && stop_on_entry_) {
    it->second.addBreakPoint(1);
    entry_path_ = normalized_path;
  }
}

void DebugBridge::setBreakPoints(
    std::string_view path,
    optional<array<SourceBreakpoint>> breakpoints) {
  std::string normalized_path = normalizePath(path);
  task_pool_.post([this, normalized_path = std::move(normalized_path),
                   breakpoints = std::move(breakpoints)] {
    // Clear all breakpoints
    if (!breakpoints.has_value()) {
      DEBUGGER_LOG_INFO("[setBreakPoint] clear all breakpoints: {}",
                        normalized_path);
      auto it = files_.find(normalized_path);
      if (it == files_.end())
        return;
      it->second.clearBreakPoints();
      return;
    }

    auto it = files_.find(normalized_path);
    std::unordered_map<int, BreakPoint> bps;
    for (const auto& breakpoint : *breakpoints) {
      auto bp = BreakPoint::create(breakpoint.line);
      if (breakpoint.condition.has_value())
        bp.setCondition(breakpoint.condition.value());
      bps.emplace(static_cast<int>(breakpoint.line), std::move(bp));
    }

    if (it == files_.end()) {
      DEBUGGER_LOG_INFO("[setBreakPoint] create new file with breakpoints: {}",
                        normalized_path);

      File file;
      file.setPath(normalized_path);
      it = files_.emplace(normalized_path, File()).first;
    } else
      DEBUGGER_LOG_INFO("[setBreakPoint] file already loaded: {}",
                        normalized_path);

    auto& file = it->second;
    file.setBreakPoints(bps);
  });
}

std::string DebugBridge::normalizePath(std::string_view path) const {
  if (path.empty())
    return std::string{};
  std::string prefix_removed =
      std::string((path[0] == '@' || path[0] == '=') ? path.substr(1) : path);
  if (std::filesystem::path(prefix_removed).is_relative())
    prefix_removed = lua_root_ + "/" + prefix_removed;
  std::string with_extension =
      std::filesystem::path(prefix_removed).replace_extension(".lua").string();
  return std::filesystem::weakly_canonical(with_extension).string();
}

BreakContext DebugBridge::getBreakContext(lua_State* L) const {
  lua_Debug ar;
  lua_getinfo(L, 0, "sl", &ar);
  return BreakContext{
      .source_ = ar.source,
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

bool DebugBridge::isBreakOnEntry(lua_State* L) {
  auto context = getBreakContext(L);
  auto it = files_.find(normalizePath(entry_path_));
  if (it == files_.end())
    return false;
  auto* bp = it->second.findBreakPoint(1);
  if (bp == nullptr)
    return false;

  return entry_path_ == normalizePath(context.source_) &&
         context.line_ == bp->targetLine();
}

void DebugBridge::interruptUpdate() {
  task_pool_.process();
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
  task_pool_.post([this] { clearBreakPoints(); });

  processSingleStep(nullptr);
  resumeInternal();
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
  if (break_vm_ == nullptr)
    return;
  lua_State* L = vm_registry_.getRoot(break_vm_);
  auto callbacks = lua_callbacks(L);
  if (!enable) {
    callbacks->debugstep = nullptr;
    return;
  }

  callbacks->debugstep = LuaStatics::debugstep;
}

void DebugBridge::resumeInternal() {
  {
    std::unique_lock<std::mutex> lock(break_mutex_);
    DEBUGGER_LOG_INFO("[resume] Resume execution");
    resume_ = true;
  }

  resume_cv_.notify_one();
}

ResponseOrError<EvaluateResponse> DebugBridge::evaluate(
    const EvaluateRequest& request) {
  if (!isDebugBreak())
    return Error{"Evaluate request is not allowed when not in debug break"};

  if (!request.context.has_value())
    return Error{"Evaluate request must have context"};

  ResponseOrError<EvaluateResponse> response;
  executeInMainThread([&] {
    auto context = request.context.value();
    DEBUGGER_LOG_INFO("[evaluate] Evaluate context: {}", context);

    if (context == "repl")
      response = evaluateRepl(request);
    else if (context == "watch")
      response = evaluateWatch(request);
    else {
      DEBUGGER_LOG_ERROR("[evaluate] Invalid evaluate context: {}", context);
      response = Error{"Invalid evaluate context"};
    }
  });
  return response;
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

  DEBUGGER_ASSERT(level >= 0 && level < vm_stack_.size());
  lua_State* L = vm_stack_[level];

  if (!lua_utils::pushBreakEnv(L, level))
    return Error{"Failed to push break environment"};

  auto ret = lua_utils::eval(L, request.expression, -1);
  if (!ret.has_value()) {
    auto error = lua_utils::type::toString(L, -1);
    lua_pop(L, 2);
    return Error{error};
  }

  EvaluateResponse response;

  std::string result;
  for (int i = *ret; i >= 1; --i) {
    if (!response.type.has_value()) {
      response.type = lua_utils::type::getTypeName(lua_type(L, -i));

      if (lua_istable(L, -i) || lua_isuserdata(L, -i)) {
        auto scope = lua_istable(L, -i) ? Scope::createTable(L, -i)
                                        : Scope::createUserData(L, -i);
        variable_registry_.registerVariables(scope, {});
        response.variablesReference = scope.getKey();
      }
    }

    result += lua_utils::type::toString(L, -i);
    if (i != 1)
      result += "\n";
  }

  // Pop result
  if (*ret > 0)
    lua_pop(L, *ret);

  // Pop the environment
  lua_pop(L, 1);

  response.result = result;
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

bool DebugBridge::hitBreakPoint(lua_State* L) {
  auto* bp = findBreakPoint(L);
  if (bp == nullptr)
    return true;

  auto hit_result = bp->hit(L);
  if (hit_result.isError()) {
    // Encountered error when evaluating breakpoint condition
    writeDebugConsole(
        log::formatError("Failed to evaluate breakpoint condition: {}",
                         hit_result.error()),
        L);
    return true;
  }

  return hit_result.value();
}

BreakPoint* DebugBridge::findBreakPoint(lua_State* L) {
  lua_Debug ar;
  lua_getinfo(L, 0, "sl", &ar);
  auto it = files_.find(normalizePath(ar.source));
  if (it == files_.end())
    return nullptr;

  auto& file = it->second;
  return file.findBreakPoint(ar.currentline);
}

void DebugBridge::refetchVariables() {
  variable_registry_.clear();
  updateVariables();
}

void DebugBridge::updateVariables() {
  executeInMainThread(
      [&] { variable_registry_.update(vm_registry_.getAncestors(break_vm_)); });
}

std::vector<StackFrame> DebugBridge::refetchStackFrames() {
  std::vector<StackFrame> frames;
  lua_Debug ar;
  lua_State* L = break_vm_;
  vm_stack_.clear();
  int depth = 0;
  while (L != nullptr) {
    for (int level = 0; lua_getinfo(L, level, "sln", &ar); ++level) {
      if (ar.what[0] == 'C')
        continue;
      StackFrame frame;
      frame.id = depth++;
      frame.name = ar.name ? ar.name : "anonymous";
      frame.source = Source{};
      if (ar.source)
        frame.source->path = normalizePath(ar.source);
      frame.line = ar.currentline;
      frames.emplace_back(std::move(frame));
      vm_stack_.push_back(L);
    }
    L = vm_registry_.getParent(L);
  }

  return frames;
}

void DebugBridge::mainThreadWait(lua_State* L,
                                 std::unique_lock<std::mutex>& lock) {
  break_vm_ = L;
  resume_ = false;
  while (!resume_) {
    resume_cv_.wait(lock, [this] { return resume_ || main_fn_ != nullptr; });

    // Execute main_fn_ if it's not empty
    if (main_fn_ != nullptr) {
      main_fn_();
      main_fn_ = nullptr;
      resume_cv_.notify_one();
    }
  }
  break_vm_ = nullptr;
}

void DebugBridge::executeInMainThread(std::function<void()> fn) {
  DEBUGGER_ASSERT(isDebugBreak());

  // Fill the main_fn_ and wait for it to be executed
  std::unique_lock<std::mutex> lock(break_mutex_);
  main_fn_ = std::move(fn);
  resume_cv_.notify_one();
  resume_cv_.wait(lock, [this] { return main_fn_ == nullptr; });
}

}  // namespace luau::debugger