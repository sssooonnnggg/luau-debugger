#include <lapi.h>
#include <lobject.h>
#include <lstate.h>
#include <lua.h>
#include <format>

#include "internal/utils.h"
#include "internal/variable.h"
#include "internal/variable_registry.h"

namespace luau::debugger {
Variable::Variable(VariableRegistry* registry,
                   lua_State* L,
                   std::string_view name)
    : L_(L), name_(name) {
  type_ = lua_type(L, -1);
  value_ = lua_utils::getDisplayValue(L, -1);
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
    std::string field_name = lua_utils::getDisplayValue(L, -2);
    variables.emplace_back(registry->createVariable(L, field_name));
    lua_pop(L, 1);
  }
}

}  // namespace luau::debugger