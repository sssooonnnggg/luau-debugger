#include <lua.h>
#include <format>

#include "internal/variable.h"
#include "internal/variable_registry.h"
#include "lapi.h"
#include "lobject.h"
#include "lstate.h"

namespace {
std::string getDisplayValue(lua_State* L, int index) {
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

}  // namespace

namespace luau::debugger {
Variable::Variable(VariableRegistry* registry,
                   lua_State* L,
                   std::string_view name)
    : L_(L), name_(name) {
  type_ = lua_type(L, -1);
  value_ = getDisplayValue(L, -1);
  registryTableFields(registry, L);
}

int Variable::getKey() const {
  return key_;
}

bool Variable::isTable() const {
  return type_ == LUA_TTABLE;
}

std::string_view Variable::getName() const {
  return name_;
}

std::string_view Variable::getValue() const {
  return value_;
}

void Variable::registryTableFields(luau::debugger::VariableRegistry* registry,
                                   lua_State* L) {
  if (!isTable())
    return;

  const TValue* t = luaA_toobject(L, -1);
  auto* table_address = hvalue(t);

  key_ = RegistryKey(table_address).key_;

  if (registry->isRegistered(key_))
    return;

  auto [it, _] = registry->registerVariables(key_, {});
  auto& variables = it->second;

  lua_checkstack(L, 2);
  lua_pushnil(L);
  while (lua_next(L, -2)) {
    std::string field_name = getDisplayValue(L, -2);
    variables.emplace_back(registry->createVariable(L, field_name));
    lua_pop(L, 1);
  }
}

}  // namespace luau::debugger