#pragma once
#include <optional>
#include <string>

#include <lua.h>

#include <internal/scope.h>

namespace luau::debugger {
class VariableRegistry;
class Variable {
 public:
  Scope getScope() const;
  bool isTable() const;
  bool isUserData() const;
  bool hasFields() const;
  std::string_view getName() const;
  std::string_view getValue() const;
  std::string getType() const;

  std::string setValue(Scope scope, const std::string& value);

  static void loadFields(luau::debugger::VariableRegistry* registry,
                         const Scope& scope);

 private:
  friend class VariableRegistry;
  Variable(VariableRegistry* registry,
           lua_State* L,
           std::string_view name,
           int level);
  void addScope(luau::debugger::VariableRegistry* registry, lua_State* L);

  static void addRawFields(luau::debugger::VariableRegistry* registry,
                           lua_State* L,
                           const Scope& scope,
                           int value_idx);
  static void addIterFields(luau::debugger::VariableRegistry* registry,
                            lua_State* L,
                            const Scope& scope,
                            int value_idx);
  static void addCustomFields(luau::debugger::VariableRegistry* registry,
                              lua_State* L,
                              const Scope& scope,
                              int value_idx);

  static Variable addField(lua_State* L,
                           VariableRegistry* registry,
                           const Scope& scope);

  static bool hasGetters(lua_State* L, int value_idx);

  std::string preprocess(const std::string& input_value);

 private:
  lua_State* L_ = nullptr;
  int level_ = 0;
  std::string name_;
  std::optional<int> index_ = std::nullopt;
  std::string value_;
  Scope scope_;
  int type_ = LUA_TNIL;
};
}  // namespace luau::debugger