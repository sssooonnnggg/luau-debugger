#pragma once

#include <lua.h>

namespace luau::debugger {

// Implementation of static functions provided to Lua
class LuaStatics {
 public:
  static void debugbreak(lua_State* L, lua_Debug* ar);
  static void interrupt(lua_State* L, int gc);
  static void userthread(lua_State* LP, lua_State* L);
  static void debugstep(lua_State* L, lua_Debug* ar);
  static int print(lua_State* L);
};
}  // namespace luau::debugger