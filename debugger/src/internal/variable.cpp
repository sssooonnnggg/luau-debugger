#include <lapi.h>
#include <lobject.h>
#include <lstate.h>
#include <lua.h>
#include <format>

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
  value_ = lua_utils::toString(L, -1);
  registryTableFields(registry, L);
}

Scope Variable::getScope() const {
  return scope_;
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

std::string Variable::setValue(Scope scope, const std::string& value) {
  if (!lua_utils::pushBreakEnv(L_, level_))
    throw std::runtime_error("Failed to push break environment");

  auto result = lua_utils::eval(L_, preprocessInput(value), -1);
  if (!result.has_value()) {
    auto error = lua_utils::toString(L_, -1);
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

  auto new_value = lua_utils::toString(L_, -1);
  if (scope.isTable()) {
    if (scope.pushTable()) {
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

std::string Variable::preprocessInput(const std::string& value) {
  if (type_ == LUA_TSTRING)
    return std::format("[[{}]]", value);
  else if (type_ == LUA_TVECTOR)
    return std::format("vector.create{}", value);

  return value;
}

void Variable::registryTableFields(luau::debugger::VariableRegistry* registry,
                                   lua_State* L) {
  if (!isTable())
    return;

  scope_ = Scope::createTable(L);

  if (registry->isRegistered(scope_))
    return;

  auto [it, _] = registry->registerVariables(scope_, {});
  auto& variables = it->second;

  lua_checkstack(L, 2);
  lua_pushnil(L);
  while (lua_next(L, -2)) {
    std::string field_name = lua_utils::toString(L, -2);
    auto variable = registry->createVariable(L, field_name, level_);
    if (lua_isnumber(L, -2))
      variable.index_ = lua_tointeger(L, -2);
    variables.emplace_back(variable);
    lua_pop(L, 1);
  }
}

}  // namespace luau::debugger