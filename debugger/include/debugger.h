#pragma once

#include <lua.h>
#include <functional>
#include <memory>
#include <string_view>

namespace dap {
class Session;
class ReaderWriter;
namespace net {
class Server;
}  // namespace net
}  // namespace dap

namespace luau::debugger {

namespace log {
using Logger = std::function<void(std::string_view)>;
void install(Logger info, Logger error);
}  // namespace log

class DebugBridge;

// DAP(Debug Adapter Protocol) handlers declaration
// Should be implemented in `debugger.cpp` as:
//    `void Debugger::register{Protocol}Handler()`
#define DAP_HANDLERS(HANDLER)      \
  HANDLER(Initialize)              \
  HANDLER(Launch)                  \
  HANDLER(Attach)                  \
  HANDLER(SetExceptionBreakpoints) \
  HANDLER(SetBreakpoints)          \
  HANDLER(Continue)                \
  HANDLER(Threads)                 \
  HANDLER(StackTrace)              \
  HANDLER(Scopes)                  \
  HANDLER(Variables)               \
  HANDLER(SetVariable)             \
  HANDLER(Next)                    \
  HANDLER(StepIn)                  \
  HANDLER(StepOut)                 \
  HANDLER(Evaluate)                \
  HANDLER(Pause)                   \
  HANDLER(Disconnect)

enum class DebugSession {
  Launch,
  Attach,
};
class Debugger {
 public:
  Debugger(bool stop_on_entry);
  ~Debugger();

  void initialize(lua_State* L);

  // NOTE: this function should be called before `lua_close`
  void release(lua_State* L);

  void setRoot(std::string_view root);

  bool listen(int port);
  bool stop();

  void onLuaFileLoaded(lua_State* L, std::string_view path, bool is_entry);
  void onError(std::string_view msg, lua_State* L);

 private:
  void onClientConnected(const std::shared_ptr<dap::ReaderWriter>& rw);
  void onClientError(const char* msg);
  void onSessionError(const char* msg);

  void registerProtocolHandlers();

#define REGISTER_HANDLER(NAME) void register##NAME##Handler();
  DAP_HANDLERS(REGISTER_HANDLER)
#undef REGISTER_HANDLER

  void closeSession();
  void invalidateVariables();

 private:
  friend class DebugBridge;

  std::unique_ptr<dap::net::Server> server_;
  std::unique_ptr<dap::Session> session_;

  std::unique_ptr<DebugBridge> debug_bridge_;

  DebugSession session_type_ = DebugSession::Attach;
};

}  // namespace luau::debugger