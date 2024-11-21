#pragma once

#include <dap/protocol.h>
#include <dap/typeof.h>
#include <nlohmann_json_serializer.h>
#include <concepts>
#include <exception>

#include <Luau/Common.h>
#include <Luau/Compiler.h>
#include <lua.h>

#include "Luau/BytecodeBuilder.h"
#include "internal/log.h"
#include "log.h"

namespace luau::debugger::utils {

template <class T>
concept Serializable =
    std::derived_from<T, dap::Request> || std::derived_from<T, dap::Event> ||
    std::derived_from<T, dap::Response>;

template <Serializable T>
inline std::string toString(const T& t) {
  dap::json::NlohmannSerializer s;
  dap::TypeOf<T>::type()->serialize(&s, reinterpret_cast<const void*>(&t));
  return s.dump();
}

}  // namespace luau::debugger::utils

namespace luau::debugger::lua_utils {

inline int doString(lua_State* L, const std::string& code, int env) {
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
      return 1;
    }
  }

  auto bytecode = bcb.getBytecode();
  int result = luau_load(L, "repl", bytecode.data(), bytecode.size(), 0);
  if (result != 0)
    lua_pop(L, 1);

  lua_pushvalue(L, env_idx);
  lua_setfenv(L, -2);

  int call_result = lua_pcall(L, 0, LUA_MULTRET, 0);
  if (call_result == LUA_OK)
    return lua_gettop(L) - top;
  else {
    DEBUGGER_LOG_ERROR("Error running code: {}", lua_tostring(L, -1));
    return 1;
  }
}

inline std::string getDisplayValue(lua_State* L, int index) {
  auto type = lua_type(L, index);
  std::string value;
  switch (type) {
    case LUA_TNIL:
      value = "nil";
      break;
    case LUA_TBOOLEAN:
      value = lua_toboolean(L, index) ? "true" : "false";
      break;
    case LUA_TVECTOR: {
      const float* v = lua_tovector(L, index);
      value = std::format("({}, {}, {})", v[0], v[1], v[2]);
      break;
    }
    case LUA_TNUMBER:
      // NOTICE: make lua_tostring call to a copy of the value to avoid modify
      // the original value
      lua_checkstack(L, 1);
      lua_pushvalue(L, index);
      value = lua_tostring(L, -1);
      lua_pop(L, 1);
      break;
    case LUA_TSTRING:
      value = lua_tostring(L, index);
      break;
    case LUA_TLIGHTUSERDATA:
      value = "lightuserdata";
      break;
    case LUA_TTABLE:
      value = "table";
      break;
    case LUA_TFUNCTION:
      value = "function";
      break;
    case LUA_TUSERDATA:
      value = "userdata";
      break;
    case LUA_TTHREAD:
      value = "thread";
      break;
    case LUA_TBUFFER:
      value = "buffer";
      break;
    default:
      value = "unknown";
      break;
  };
  return value;
}
}  // namespace luau::debugger::lua_utils