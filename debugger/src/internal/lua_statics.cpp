#include <lstate.h>
#include <lua.h>
#include <lualib.h>

#include <dap/protocol.h>
#include <dap/types.h>

#include <dap/session.h>
#include <internal/breakpoint.h>
#include <internal/debug_bridge.h>
#include <internal/file.h>
#include <internal/log.h>
#include <internal/utils.h>
#include <internal/variable.h>
#include "internal/utils/lua_utils.h"

#include "lua_statics.h"

namespace luau::debugger {

namespace {
class StackPusher {
 public:
  StackPusher(lua_State* L) : L_(L) {
    auto bridge = DebugBridge::get(L_);
    if (bridge == nullptr)
      return;
    bridge->vms().pushStack(L_);
  }
  ~StackPusher() {
    auto bridge = DebugBridge::get(L_);
    if (bridge == nullptr)
      return;
    bridge->vms().popStack();
  }

 private:
  lua_State* L_;
};
}  // namespace

void LuaStatics::debugbreak(lua_State* L, lua_Debug* ar) {
  auto bridge = DebugBridge::get(L);
  if (bridge == nullptr)
    return;
  bridge->onDebugBreak(L, ar,
                       bridge->isBreakOnEntry(L)
                           ? DebugBridge::BreakReason::Entry
                           : DebugBridge::BreakReason::BreakPoint);
};

void LuaStatics::interrupt(lua_State* L, int gc) {
  auto bridge = DebugBridge::get(L);
  if (bridge == nullptr)
    return;
  bridge->interruptUpdate(L);
};

void LuaStatics::userthread(lua_State* LP, lua_State* L) {
  auto bridge = DebugBridge::get(L);
  if (bridge == nullptr)
    return;

  if (LP == nullptr)
    bridge->vms().markDead(L);
  else
    bridge->vms().markAlive(L, LP);
};

void LuaStatics::debugstep(lua_State* L, lua_Debug* ar) {
  auto bridge = DebugBridge::get(L);
  if (bridge == nullptr)
    return;
  if (bridge->single_step_processor_ != nullptr)
    if ((bridge->single_step_processor_)(L, ar))
      bridge->onDebugBreak(L, ar, DebugBridge::BreakReason::Step);
};

int LuaStatics::print(lua_State* L) {
  int lua_print = lua_upvalueindex(1);

  int args = lua_gettop(L);
  int begin = lua_absindex(L, -args);
  int end = lua_absindex(L, -1) + 1;

  lua_checkstack(L, args + 1);
  lua_pushvalue(L, lua_print);
  for (int i = begin; i < end; ++i)
    lua_pushvalue(L, i);

  lua_call(L, args, 0);

  auto bridge = DebugBridge::get(L);
  if (bridge == nullptr)
    return 0;

  int n = lua_gettop(L);
  std::string output;
  for (int i = 1; i <= n; i++) {
    size_t l;
    const char* s = luaL_tolstring(L, i, &l);
    if (i > 1)
      output += "\t";
    output.append(s, l);
    lua_pop(L, 1);  // pop result
  }
  output += "\n";

  bridge->writeDebugConsole(output, L, 1);
  return 0;
}

int LuaStatics::breakHere(lua_State* L) {
  auto bridge = DebugBridge::get(L);
  if (bridge == nullptr)
    return 0;

  bridge->onDebugBreak(L, nullptr, DebugBridge::BreakReason::Pause);
  return 0;
}

int LuaStatics::cowrap(lua_State* L) {
  int top = lua_gettop(L);
  lua_checkstack(L, 1 + top);
  lua_pushvalue(L, lua_upvalueindex(1));
  for (int i = 1; i <= top; ++i)
    lua_pushvalue(L, i);
  lua_call(L, top, 1);

  if (auto* cl = lua_utils::getCFunction(L, -1)) {
    auto* cont = cl->c.cont;
    lua_checkstack(L, 1);
    lua_pushcclosurek(
        L,
        [](lua_State* L) {
          StackPusher _(L);
          return forward(L, lua_upvalueindex(1));
        },
        nullptr, 1, cont);
  }
  return 1;
}

int LuaStatics::coresume(lua_State* L) {
  StackPusher _(L);
  return forward(L, lua_upvalueindex(1));
}

int LuaStatics::forward(lua_State* L, int index) {
  int top = lua_gettop(L);
  lua_checkstack(L, 1 + top);
  lua_pushvalue(L, index);
  for (int i = 1; i <= top; ++i)
    lua_pushvalue(L, i);
  lua_call(L, top, LUA_MULTRET);
  return lua_gettop(L) - top;
}

};  // namespace luau::debugger