#include <optional>

#include <lapi.h>
#include <lobject.h>
#include <lstate.h>
#include <lua.h>

#include <internal/utils.h>
#include <internal/utils/lua_types.h>
#include <internal/utils/lua_utils.h>

namespace luau::debugger::lua_utils {

std::optional<int> eval(lua_State* L, const std::string& code, int env) {
  DisableDebugStep _(L);

  lua_setsafeenv(L, LUA_ENVIRONINDEX, false);
  auto env_idx = lua_absindex(L, env);
  int top = lua_gettop(L);

  Luau::BytecodeBuilder bcb;
  try {
    Luau::compileOrThrow(bcb, std::string("return ") + code);
  } catch (const std::exception&) {
    try {
      Luau::compileOrThrow(bcb, code);
    } catch (const std::exception& e) {
      DEBUGGER_LOG_ERROR("Error compiling code: {}", e.what());
      lua_pushstring(L, e.what());
      return std::nullopt;
    }
  }

  auto bytecode = bcb.getBytecode();
  int result = luau_load(L, code.c_str(), bytecode.data(), bytecode.size(), 0);
  if (result != 0)
    return std::nullopt;

  lua_pushvalue(L, env_idx);
  lua_setfenv(L, -2);

  int call_result = lua_pcall(L, 0, LUA_MULTRET, 0);
  if (call_result == LUA_OK)
    return lua_gettop(L) - top;
  else {
    DEBUGGER_LOG_ERROR("Error running code: {}", lua_tostring(L, -1));
    return std::nullopt;
  }
}

std::string toString(lua_State* L, int index) {
  return type::toString(L, index);
}

bool pushBreakEnv(lua_State* L, int level) {
  lua_Debug ar;
  lua_checkstack(L, 10);

  // Create new table for break environment
  lua_newtable(L);

  // Push function at level
  if (!lua_getinfo(L, level, "f", &ar)) {
    DEBUGGER_LOG_ERROR("[pushBreakEnv] Failed to get function info at level {}",
                       level);
    lua_pop(L, 1);
    return false;
  }

  // Get function env
  lua_getfenv(L, -1);

  // -1: function env table
  // -2: function
  // -3: break env table
  int break_env = lua_absindex(L, -3);
  int fenv = lua_absindex(L, -1);

  // set fenv as metatable
  lua_newtable(L);
  lua_pushstring(L, "__index");
  lua_pushvalue(L, fenv);
  lua_rawset(L, -3);
  lua_setmetatable(L, break_env);
  lua_pop(L, 1);

  // -1: function
  // -2: env table

  int index = 1;
  while (auto* name = lua_getlocal(L, level, index++)) {
    lua_pushstring(L, name);
    lua_insert(L, -2);

    // -1: value
    // -2: key
    // -3: function
    // -4: table
    lua_rawset(L, -4);
  }

  index = 1;
  while (auto* name = lua_getupvalue(L, -1, index++)) {
    lua_pushstring(L, name);
    lua_insert(L, -2);
    lua_rawset(L, -4);
  }

  // Pop function
  lua_pop(L, 1);

  return true;
}

bool setLocal(lua_State* L, int level, const std::string& name, int index) {
  auto value_idx = lua_absindex(L, index);
  int n = 1;
  while (const char* local_name = lua_getlocal(L, level, n)) {
    if (name == local_name) {
      lua_pushvalue(L, value_idx);
      auto* result = lua_setlocal(L, level, n);
      lua_pop(L, 1);
      return result != nullptr;
    }
    lua_pop(L, 1);
    ++n;
  }
  return false;
}

bool setUpvalue(lua_State* L, int level, const std::string& name, int index) {
  auto value_idx = lua_absindex(L, index);
  lua_Debug ar;
  lua_checkstack(L, 1);
  if (!lua_getinfo(L, level, "f", &ar)) {
    DEBUGGER_LOG_ERROR("[setUpvalue] Failed to get function info at level {}",
                       level);
    return false;
  }
  auto function_index = lua_absindex(L, -1);

  int n = 1;
  while (const char* local_name = lua_getupvalue(L, function_index, n)) {
    if (name == local_name) {
      lua_pushvalue(L, value_idx);
      auto* result = lua_setupvalue(L, function_index, n);
      DEBUGGER_ASSERT(result != nullptr);
      lua_pop(L, 2);
      return result != nullptr;
    }
    lua_pop(L, 1);
    ++n;
  }
  lua_pop(L, 1);
  return false;
}

Closure* getFunction(lua_State* L, int index) {
  auto o = luaA_toobject(L, index);
  return isLfunction(o) ? clvalue(o) : nullptr;
}

}  // namespace luau::debugger::lua_utils
