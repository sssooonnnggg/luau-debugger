#include <lua.h>
#include <string_view>

#include <internal/log.h>
#include <internal/scope.h>
#include <internal/variable_registry.h>

namespace luau::debugger {

Variable VariableRegistry::createVariable(lua_State* L,
                                          std::string_view name,
                                          int level) {
  return Variable(this, L, name, level);
}

std::pair<VariableRegistry::Iter, bool> VariableRegistry::registerVariables(
    Scope scope,
    std::vector<Variable> variables) {
  auto result = variables_.try_emplace(scope, std::move(variables));
  if (!result.second)
    DEBUGGER_LOG_ERROR("Variable already registered: {}",
                       result.first->first.getName());
  return result;
}

bool VariableRegistry::isRegistered(Scope scope) const {
  return variables_.find(scope) != variables_.end();
}

std::vector<Variable>* VariableRegistry::getVariables(Scope scope) {
  auto it = variables_.find(scope);
  if (it == variables_.end()) {
    DEBUGGER_LOG_ERROR("Variable not found: {}", scope.getKey());
    return nullptr;
  }

  return &it->second;
}

std::pair<const Scope, std::vector<Variable>>* VariableRegistry::getVariables(
    int reference) {
  Scope scope(reference);
  auto it = variables_.find(scope);
  if (it == variables_.end()) {
    DEBUGGER_LOG_ERROR("Variable not found: {}", scope.getKey());
    return nullptr;
  }

  return &(*it);
}

void VariableRegistry::clear() {
  variables_.clear();
  depth_ = 0;
}

void VariableRegistry::update(lua_State* L) {
  int stack_depth = lua_stackdepth(L);

  for (int level = 0; level < stack_depth; ++level, ++depth_)
    updateStack(L, level);
}

Scope VariableRegistry::getLocalScope(int level) {
  return Scope::createLocal(std::format("___locals__{}", level));
}

Scope VariableRegistry::getUpvalueScope(int level) {
  return Scope::createUpvalue(std::format("___upvalues__{}", level));
}

void VariableRegistry::updateStack(lua_State* L, int level) {
  // Register local variables
  std::vector<Variable> variables;
  int index = 1;
  while (const char* name = lua_getlocal(L, level, index++)) {
    variables.emplace_back(createVariable(L, name, level));
    lua_pop(L, 1);
  }
  registerVariables(getLocalScope(depth_), variables);

  // Register upvalues
  lua_Debug ar = {};
  lua_getinfo(L, level, "f", &ar);
  std::vector<Variable> upvalues;
  index = 1;
  while (const char* name = lua_getupvalue(L, -1, index++)) {
    upvalues.emplace_back(createVariable(L, name, level));
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
  registerVariables(getUpvalueScope(depth_), upvalues);
}

std::vector<Variable>* VariableRegistry::getLocals(int level) {
  return getVariables(getLocalScope(level));
}

std::vector<Variable>* VariableRegistry::getUpvalues(int level) {
  return getVariables(getUpvalueScope(level));
}

}  // namespace luau::debugger