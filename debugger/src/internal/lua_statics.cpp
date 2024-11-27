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

#include "lua_statics.h"

namespace luau::debugger {

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
  bridge->interruptUpdate();
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
};  // namespace luau::debugger