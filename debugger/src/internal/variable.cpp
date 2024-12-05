#include <format>
#include "internal/scope.h"
#include "internal/utils/lua_utils.h"

#include <lapi.h>
#include <lobject.h>
#include <lstate.h>
#include <lua.h>
#include <lualib.h>

#include <internal/log.h>
#include <internal/utils.h>
#include <internal/variable.h>
#include <internal/variable_registry.h>

namespace luau::debugger {
Variable::Variable(VariableRegistry* registry,
                   lua_State* L,
                   std::string_view name,
                   int level)
    : L_(L), name_(name), level_(level) {
  type_ = lua_type(L, -1);
  value_ = lua_utils::type::toString(L, -1);
  addScope(registry, L);
}

Scope Variable::getScope() const {
  return scope_;
}

bool Variable::isTable() const {
  return type_ == LUA_TTABLE;
}

bool Variable::isUserData() const {
  return type_ == LUA_TUSERDATA;
}

bool Variable::hasFields() const {
  return isTable() || isUserData();
}

std::string_view Variable::getName() const {
  return name_;
}

std::string_view Variable::getValue() const {
  return value_;
}

std::string Variable::getType() const {
  return lua_utils::type::getTypeName(type_);
}

std::string Variable::setValue(Scope scope, const std::string& value) {
  if (!lua_utils::pushBreakEnv(L_, level_))
    throw std::runtime_error("Failed to push break environment");

  auto result = lua_utils::eval(L_, preprocess(value), -1);
  if (!result.has_value()) {
    auto error = lua_utils::type::toString(L_, -1);
    lua_pop(L_, 2);
    throw std::runtime_error(error);
  }

  auto ret_count = result.value();
  if (ret_count == 0) {
    lua_pop(L_, 1);
    return value_;
  }

  if (ret_count > 1)
    lua_pop(L_, ret_count - 1);

  auto new_value = lua_utils::type::toString(L_, -1);
  if (scope.isTable()) {
    if (scope.pushRef()) {
      // -1: key
      // -2: table
      // -3: value
      // -4: env
      lua_checkstack(L_, 2);
      if (index_.has_value())
        lua_pushinteger(L_, index_.value());
      else
        lua_pushstring(L_, name_.data());
      lua_pushvalue(L_, -3);
      lua_settable(L_, -3);
      lua_pop(L_, 3);
    }
  } else if (scope.isLocal()) {
    if (!lua_utils::setLocal(L_, level_, name_, -1)) {
      lua_pop(L_, 2);
      throw std::runtime_error("Failed to set local variable");
    }
  } else if (scope.isUpvalue()) {
    if (!lua_utils::setUpvalue(L_, level_, name_, -1)) {
      lua_pop(L_, 2);
      throw std::runtime_error("Failed to set upvalue");
    }
  } else {
    lua_pop(L_, 2);
    throw std::runtime_error("Invalid scope");
  }

  lua_pop(L_, 2);
  return new_value;
}

std::string Variable::preprocess(const std::string& input_value) {
  if (type_ == LUA_TSTRING)
    return std::format("[[{}]]", input_value);
  else if (type_ == LUA_TVECTOR)
    return std::format("vector.create{}", input_value);

  return input_value;
}

void Variable::addScope(luau::debugger::VariableRegistry* registry,
                        lua_State* L) {
  if (!hasFields())
    return;

  if (isTable())
    scope_ = Scope::createTable(L);
  else if (isUserData())
    scope_ = Scope::createUserData(L);

  scope_.setName(name_);
  scope_.setLevel(level_);

  if (!registry->isRegistered(scope_))
    registry->registerVariables(scope_, {});
}

void Variable::loadFields(luau::debugger::VariableRegistry* registry,
                          const Scope& scope) {
  auto* L = scope.getLuaState();
  lua_utils::StackGuard guard(L);
  if (!scope.pushRef())
    return;

  int value_idx = lua_absindex(L, -1);

  // https://github.com/luau-lang/rfcs/blob/master/docs/generalized-iteration.md
  if (luaL_getmetafield(L, value_idx, "__iter"))
    addIterFields(registry, L, scope, value_idx);
  else if (scope.isTable())
    addRawFields(registry, L, scope, value_idx);
}

void Variable::addRawFields(luau::debugger::VariableRegistry* registry,
                            lua_State* L,
                            const Scope& scope,
                            int value_idx) {
  auto* variables = registry->getVariables(scope, false);
  if (!variables)
    return;

  lua_pushnil(L);
  while (lua_next(L, value_idx)) {
    variables->emplace_back(addField(L, registry, scope));
    lua_pop(L, 1);
  }
}

void Variable::addIterFields(luau::debugger::VariableRegistry* registry,
                             lua_State* L,
                             const Scope& scope,
                             int value_idx) {
  auto* variables = registry->getVariables(scope, false);
  if (!variables)
    return;

  lua_pushvalue(L, value_idx);
  int call_result = lua_pcall(L, 1, 3, 0);
  if (call_result != LUA_OK) {
    DEBUGGER_LOG_ERROR(
        "[Variable::registryFields] Failed to call __iter for {}, error: {}",
        scope.getName(), lua_tostring(L, -1));
    return;
  }

  auto next = lua_absindex(L, -3);
  auto state = lua_absindex(L, -2);
  auto init = lua_absindex(L, -1);

  lua_pushvalue(L, next);
  lua_pushvalue(L, state);
  lua_pushvalue(L, init);
  while (true) {
    if (LUA_OK != lua_pcall(L, 2, 2, 0)) {
      DEBUGGER_LOG_ERROR(
          "[Variable::registryFields] Failed to call __iter for {}, error: {}",
          scope.getName(), lua_tostring(L, -1));
      return;
    }
    if (lua_isnil(L, -2))
      return;

    variables->emplace_back(addField(L, registry, scope));

    // pop value
    lua_pop(L, 1);

    // prepare for next iteration
    lua_pushvalue(L, next);
    lua_pushvalue(L, state);
    lua_pushvalue(L, -3);
    lua_remove(L, -4);
  }
}

Variable Variable::addField(lua_State* L,
                            VariableRegistry* registry,
                            const Scope& scope) {
  bool number_index = lua_isnumber(L, -2);
  std::string field_name = lua_utils::type::toString(L, -2);
  if (number_index)
    field_name = std::format("[{}]", lua_tointeger(L, -2));
  auto variable = registry->createVariable(L, field_name, scope.getLevel());
  if (lua_isnumber(L, -2))
    variable.index_ = lua_tointeger(L, -2);
  return variable;
}

}  // namespace luau::debugger