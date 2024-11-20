#pragma once

#include <lua.h>
#include <string>

namespace luau::debugger {
class VariableRegistry;
class Variable {
 public:
  Variable(VariableRegistry* registry, lua_State* L, std::string_view name);
  int getKey() const;
  bool isTable() const;
  std::string_view getName() const;
  std::string_view getValue() const;

 private:
  void registryTableFields(luau::debugger::VariableRegistry* registry,
                           lua_State* L);

  lua_State* L_ = nullptr;
  std::string name_;
  std::string value_;
  int key_ = 0;
  int type_ = LUA_TNIL;
};
}  // namespace luau::debugger