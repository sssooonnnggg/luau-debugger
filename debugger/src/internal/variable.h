#pragma once

#include <lua.h>
#include <string>

#include <internal/scope.h>

namespace luau::debugger {
class VariableRegistry;
class Variable {
 public:
  Scope getScope() const;
  bool isTable() const;
  std::string_view getName() const;
  std::string_view getValue() const;

  std::string setValue(Scope scope, const std::string& value);

 private:
  friend class VariableRegistry;
  Variable(VariableRegistry* registry,
           lua_State* L,
           std::string_view name,
           int level);
  void registryTableFields(luau::debugger::VariableRegistry* registry,
                           lua_State* L);
  std::string preprocessInput(const std::string& value);

 private:
  lua_State* L_ = nullptr;
  int level_ = 0;
  std::string name_;
  std::string value_;
  Scope scope_;
  int type_ = LUA_TNIL;
};
}  // namespace luau::debugger