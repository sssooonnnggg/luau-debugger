#pragma once

#include <string_view>
#include <unordered_map>

#include <lua.h>
#include <vector>

#include <internal/scope.h>
#include <internal/variable.h>

namespace luau::debugger {

class VariableRegistry {
 public:
  void update(lua_State* L);

  std::vector<Variable>* getLocals(int level);
  std::vector<Variable>* getUpvalues(int level);

  Scope getLocalScope(int level);
  Scope getUpvalueScope(int level);

  Variable createVariable(lua_State* L, std::string_view name, int level);

  using Iter = std::unordered_map<Scope, std::vector<Variable>>::iterator;
  std::pair<Iter, bool> registerVariables(Scope scope,
                                          std::vector<Variable> variables);
  bool isRegistered(Scope scope) const;
  std::vector<Variable>* getVariables(Scope scope);
  std::pair<const Scope, std::vector<Variable>>* getVariables(int reference);

 private:
  void updateStack(lua_State* L, int level);

 private:
  std::unordered_map<Scope, std::vector<Variable>> variables_;
};

}  // namespace luau::debugger