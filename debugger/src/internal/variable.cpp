#include <format>

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
  addFields(registry, L);
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

  auto result = lua_utils::eval(L_, preprocessInput(value), -1);
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

std::string Variable::preprocessInput(const std::string& value) {
  if (type_ == LUA_TSTRING)
    return std::format("[[{}]]", value);
  else if (type_ == LUA_TVECTOR)
    return std::format("vector.create{}", value);

  return value;
}

void Variable::addFields(luau::debugger::VariableRegistry* registry,
                         lua_State* L) {
  if (!hasFields())
    return;

  if (isTable())
    scope_ = Scope::createTable(L);
  else if (isUserData())
    scope_ = Scope::createUserData(L);

  if (registry->isRegistered(scope_))
    return;

  lua_utils::StackGuard guard(L);
  int value_idx = lua_absindex(L, -1);

  // https://github.com/luau-lang/rfcs/blob/master/docs/generalized-iteration.md
  if (luaL_getmetafield(L, value_idx, "__iter"))
    addIterFields(registry, L, value_idx);
  else if (isTable())
    addRawFields(registry, L, value_idx);
}

void Variable::addRawFields(luau::debugger::VariableRegistry* registry,
                            lua_State* L,
                            int value_idx) {
  auto [it, _] = registry->registerVariables(scope_, {});
  auto& variables = it->second;

  lua_pushnil(L);
  while (lua_next(L, value_idx)) {
    variables.emplace_back(addField(L, registry));
    lua_pop(L, 1);
  }
}

void Variable::addIterFields(luau::debugger::VariableRegistry* registry,
                             lua_State* L,
                             int value_idx) {
  auto [it, _] = registry->registerVariables(scope_, {});
  auto& variables = it->second;

  lua_pushvalue(L, value_idx);
  int call_result = lua_pcall(L, 1, 3, 0);
  if (call_result != LUA_OK) {
    DEBUGGER_LOG_ERROR(
        "[Variable::registryFields] Failed to call __iter for {}, error: {}",
        name_, lua_tostring(L, -1));
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
          name_, lua_tostring(L, -1));
      return;
    }
    if (lua_isnil(L, -2))
      return;
    variables.emplace_back(addField(L, registry));

    // pop value
    lua_pop(L, 1);

    // prepare for next iteration
    lua_pushvalue(L, next);
    lua_pushvalue(L, state);
    lua_pushvalue(L, -3);
    lua_remove(L, -4);
  }
}

Variable Variable::addField(lua_State* L, VariableRegistry* registry) {
  bool number_index = lua_isnumber(L, -2);
  std::string field_name = lua_utils::type::toString(L, -2);
  if (number_index)
    field_name = std::format("[{}]", lua_tointeger(L, -2));
  auto variable = registry->createVariable(L, field_name, level_);
  if (lua_isnumber(L, -2))
    variable.index_ = lua_tointeger(L, -2);
  return variable;
}

}  // namespace luau::debugger