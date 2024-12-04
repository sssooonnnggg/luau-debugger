#include <lua.h>
#include <array>
#include <cstdlib>
#include <memory>
#include <utility>

#include <dap/io.h>
#include <dap/network.h>
#include <dap/protocol.h>
#include <dap/session.h>

#include <internal/debug_bridge.h>
#include <internal/utils.h>

#include "debugger.h"

namespace luau::debugger {

namespace log {

void install(Logger info, Logger error) {
  log::info() = std::move(info);
  log::error() = std::move(error);
}
}  // namespace log

Debugger::Debugger(bool stop_on_entry) {
  debug_bridge_ = std::make_unique<DebugBridge>(stop_on_entry);
}

Debugger::~Debugger() {
  stop();
}

void Debugger::initialize(lua_State* L) {
  lua_setthreaddata(L, reinterpret_cast<void*>(this));
  debug_bridge_->initialize(L);
}

void Debugger::setRoot(std::string_view root) {
  debug_bridge_->setRoot(root);
}

bool Debugger::listen(int port) {
  using namespace dap::net;
  server_ = dap::net::Server::create();
  if (!server_->start(
          port, [this](const auto& rw) { onClientConnected(rw); },
          [this](const char* msg) { onClientError(msg); })) {
    DEBUGGER_LOG_ERROR("Failed to start server on port {}", port);
    return false;
  }
  DEBUGGER_LOG_INFO("Listening on port {}", port);
  return true;
}

bool Debugger::stop() {
  closeSession();
  server_->stop();  
  debug_bridge_.reset();
  return true;
}

void Debugger::onLuaFileLoaded(lua_State* L,
                               std::string_view path,
                               bool is_entry) {
  debug_bridge_->onLuaFileLoaded(L, path, is_entry);
}

void Debugger::onError(std::string_view msg, lua_State* L) {
  debug_bridge_->writeDebugConsole(msg, L);
}

void Debugger::closeSession() {
  if (session_ != nullptr) {
    // FIXME: This is a workaround to avoid error message not being sent to the
    // client.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  session_ = nullptr;
}

void Debugger::registerProtocolHandlers() {
  // NOTICE: all handlers are executed in a different thread.
  DEBUGGER_LOG_INFO("Registering protocol handlers");

#define REGISTER_HANDLER(NAME) register##NAME##Handler();
  DAP_HANDLERS(REGISTER_HANDLER)
#undef REGISTER_HANDLER
}

void Debugger::registerDisconnectHandler() {
  session_->registerHandler([&](const dap::DisconnectRequest&) {
    DEBUGGER_LOG_INFO("Client closing connection");
    debug_bridge_->onDisconnect();
    return dap::DisconnectResponse{};
  });
  session_->registerSentHandler(
      [&](const dap::ResponseOrError<dap::DisconnectResponse>&) {
        if (session_type_ == DebugSession::Launch)
          std::exit(0);
      });
}

void Debugger::onClientConnected(const std::shared_ptr<dap::ReaderWriter>& rw) {
  // NOTICE: This function is called from a different thread.
  session_ = dap::Session::create();

  registerProtocolHandlers();

  session_->onError([this](const char* msg) { onSessionError(msg); });
  session_->setOnInvalidData(dap::kClose);
  session_->bind(rw);

  DEBUGGER_LOG_INFO("Debugger client connected");
}

void Debugger::onClientError(const char* msg) {
  DEBUGGER_LOG_ERROR("Debugger client error: {}", msg);
}

void Debugger::onSessionError(const char* msg) {
  DEBUGGER_LOG_ERROR("Debugger session error: {}", msg);
}

void Debugger::registerInitializeHandler() {
  session_->registerHandler([&](const dap::InitializeRequest& request) {
    DEBUGGER_LOG_INFO("Server received initialize request from client: {}",
                      dap_utils::toString(request));
    dap::InitializeResponse response;
    response.supportsReadMemoryRequest = false;
    response.supportsDataBreakpoints = false;
    response.supportsExceptionOptions = false;
    response.supportsDelayedStackTraceLoading = false;
    response.supportsSetVariable = true;
    response.supportsConditionalBreakpoints = true;
    return response;
  });
  session_->registerSentHandler(
      [&](const dap::ResponseOrError<dap::InitializeResponse>&) {
        DEBUGGER_LOG_INFO("Server sending initialized event to client");
        session_->send(dap::InitializedEvent());
        debug_bridge_->onConnect(session_.get());
      });
}

void Debugger::registerAttachHandler() {
  session_->registerHandler([&](const dap::AttachRequest& request)
                                -> dap::ResponseOrError<dap::AttachResponse> {
    DEBUGGER_LOG_INFO("Server received attach request from client: {}",
                      dap_utils::toString(request));
    session_type_ = DebugSession::Attach;
    dap::AttachResponse response;
    return response;
  });
}

void Debugger::registerLaunchHandler() {
  session_->registerHandler([&](const dap::LaunchRequest& request)
                                -> dap::ResponseOrError<dap::LaunchResponse> {
    DEBUGGER_LOG_INFO("Server received launch request from client: {}",
                      dap_utils::toString(request));
    session_type_ = DebugSession::Launch;
    dap::LaunchResponse response;
    return response;
  });
}

void Debugger::registerSetExceptionBreakpointsHandler() {
  session_->registerHandler(
      [&](const dap::SetExceptionBreakpointsRequest& request)
          -> dap::ResponseOrError<dap::SetExceptionBreakpointsResponse> {
        dap::SetExceptionBreakpointsResponse response;
        return response;
      });
}

void Debugger::registerSetBreakpointsHandler() {
  session_->registerHandler(
      [&](const dap::SetBreakpointsRequest& request)
          -> dap::ResponseOrError<dap::SetBreakpointsResponse> {
        DEBUGGER_LOG_INFO(
            "Server received setBreakpoints request from client: {}",
            dap_utils::toString(request));

        auto file_path = request.source.path;
        if (!file_path.has_value())
          return dap::Error("Invalid file path");

        debug_bridge_->setBreakPoints(file_path.value(), request.breakpoints);

        dap::SetBreakpointsResponse response;
        return response;
      });
}

void Debugger::registerContinueHandler() {
  session_->registerHandler([&](const dap::ContinueRequest&) {
    DEBUGGER_LOG_INFO("Server received continue request from client");

    // Safe to call in different thread.
    debug_bridge_->resume();
    return dap::ContinueResponse{};
  });
}

void Debugger::registerThreadsHandler() {
  session_->registerHandler([&](const dap::ThreadsRequest&) {
    DEBUGGER_LOG_INFO("Server received threads request from client");
    dap::ThreadsResponse response;
    response.threads = {dap::Thread{1, "Main Thread"}};
    return response;
  });
}

void Debugger::registerStackTraceHandler() {
  session_->registerHandler(
      [&](const dap::StackTraceRequest& request)
          -> dap::ResponseOrError<dap::StackTraceResponse> {
        DEBUGGER_LOG_INFO("Server received stackTrace request from client: {}",
                          dap_utils::toString(request));
        return debug_bridge_->getStackTrace();
      });
}

void Debugger::registerScopesHandler() {
  session_->registerHandler([&](const dap::ScopesRequest& request)
                                -> dap::ResponseOrError<dap::ScopesResponse> {
    DEBUGGER_LOG_INFO("Server received scopes request from client: {}",
                      dap_utils::toString(request));
    return debug_bridge_->getScopes(request.frameId);
  });
}

void Debugger::registerVariablesHandler() {
  session_->registerHandler(
      [&](const dap::VariablesRequest& request)
          -> dap::ResponseOrError<dap::VariablesResponse> {
        DEBUGGER_LOG_INFO("Server received variables request from client: {}",
                          dap_utils::toString(request));
        return debug_bridge_->getVariables(request.variablesReference);
      });
}

void Debugger::registerSetVariableHandler() {
  session_->registerHandler(
      [&](const dap::SetVariableRequest& request)
          -> dap::ResponseOrError<dap::SetVariableResponse> {
        DEBUGGER_LOG_INFO("Server received setVariable request from client: {}",
                          dap_utils::toString(request));
        return debug_bridge_->setVariable(request);
      });
  session_->registerSentHandler(
      [&](const dap::ResponseOrError<dap::SetVariableResponse>& res) {
        if (!res.error)
          invalidateVariables();
      });
}

void Debugger::registerNextHandler() {
  session_->registerHandler([&](const dap::NextRequest&) {
    DEBUGGER_LOG_INFO("Server received next request from client");
    debug_bridge_->stepOver();
    return dap::NextResponse{};
  });
}

void Debugger::registerStepInHandler() {
  session_->registerHandler([&](const dap::StepInRequest&) {
    DEBUGGER_LOG_INFO("Server received stepIn request from client");
    debug_bridge_->stepIn();
    return dap::StepInResponse{};
  });
}

void Debugger::registerStepOutHandler() {
  session_->registerHandler([&](const dap::StepOutRequest&) {
    DEBUGGER_LOG_INFO("Server received stepOut request from client");
    debug_bridge_->stepOut();
    return dap::StepOutResponse{};
  });
}

void Debugger::registerEvaluateHandler() {
  static bool should_invalidate = false;
  session_->registerHandler([&](const dap::EvaluateRequest& request)
                                -> dap::ResponseOrError<dap::EvaluateResponse> {
    should_invalidate =
        request.context.has_value() && request.context.value() == "repl";
    DEBUGGER_LOG_INFO("Server received evaluate request from client: {}",
                      dap_utils::toString(request));
    return debug_bridge_->evaluate(request);
  });
  session_->registerSentHandler(
      [&](const dap::ResponseOrError<dap::EvaluateResponse>& res) {
        // Invalidate variables
        if (!res.error && should_invalidate)
          invalidateVariables();
      });
}

void Debugger::invalidateVariables() {
  if (session_) {
    debug_bridge_->updateVariables();
    session_->send(
        dap::InvalidatedEvent{.areas = std::vector<std::string>{"variables"}});
  }
}

}  // namespace luau::debugger