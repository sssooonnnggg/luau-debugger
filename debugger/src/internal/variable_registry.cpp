#include <lua.h>
#include <string_view>

#include <internal/log.h>
#include <internal/scope.h>
#include <internal/utils/lua_utils.h>
#include <internal/variable_registry.h>

namespace luau::debugger {

Variable VariableRegistry::createVariable(lua_State* L,
                                          std::string_view name,
                                          int level) {
  return Variable(this, L, name, level);
}

std::vector<Variable>* VariableRegistry::registerVariables(
    Scope scope,
    std::vector<Variable> variables) {
  auto result = variables_.try_emplace(scope, std::move(variables));
  if (!result.second)
    DEBUGGER_LOG_ERROR("Variable already registered: {}",
                       result.first->first.getName());
  return &result.first->second;
}

std::vector<Variable>* VariableRegistry::registerOrUpdateVariables(
    Scope scope,
    std::vector<Variable> variables) {
  auto it = variables_.find(scope);
  if (it == variables_.end())
    return registerVariables(scope, std::move(variables));

  it->second = std::move(variables);
  return &it->second;
}

bool VariableRegistry::isRegistered(Scope scope) const {
  return variables_.find(scope) != variables_.end();
}

std::vector<Variable>* VariableRegistry::getVariables(Scope scope, bool load) {
  auto it = variables_.find(scope);
  if (it == variables_.end()) {
    DEBUGGER_LOG_ERROR("Variable not found: {}", scope.getKey());
    return nullptr;
  }

  if (load && !it->first.isLoaded()) {
    Variable::loadFields(this, it->first);
    it->first.markLoaded();
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
}

void VariableRegistry::update(
    std::vector<std::vector<lua_State*>> vm_with_ancestors) {
  if (vm_with_ancestors.empty())
    return;

  for (const auto& chain : vm_with_ancestors) {
    depth_ = 0;
    lua_State* src = chain[0];
    for (lua_State* L : chain)
      fetch(L, src);
  }

  fetchGlobals(vm_with_ancestors[0][0]);
  clearDirtyScopes();
}

void VariableRegistry::fetch(lua_State* L, lua_State* src) {
  lua_utils::StackGuard guard(L);
  lua_Debug ar;
  for (int level = 0; lua_getinfo(L, level, "sln", &ar); ++level) {
    if (ar.what[0] == 'C')
      continue;
    fetchFromStack(L, level, src);
    ++depth_;
  }
}

void VariableRegistry::fetchGlobals(lua_State* L) {
  lua_utils::StackGuard guard(L);
  lua_Debug ar;
  lua_getinfo(L, 0, "f", &ar);
  lua_getfenv(L, -1);
  std::vector<Variable> globals;
  lua_pushnil(L);
  while (lua_next(L, -2)) {
    std::string name = lua_utils::type::toString(L, -2);
    globals.emplace_back(createVariable(L, name, -1));
    lua_pop(L, 1);
  }
  registerOrUpdateVariables(getGlobalScope(), std::move(globals));
}

void VariableRegistry::clearDirtyScopes() {
  for (auto& [scope, variables] : variables_)
    if (scope.isTable() || scope.isUserData()) {
      variables.clear();
      scope.markUnloaded();
    }
}

Scope VariableRegistry::getLocalScope(lua_State* L, int level) {
  return Scope::createLocal(
      std::format("___locals__{}_{}", level, static_cast<void*>(L)));
}

Scope VariableRegistry::getUpvalueScope(lua_State* L, int level) {
  return Scope::createUpvalue(
      std::format("___upvalues__{}_{}", level, static_cast<void*>(L)));
}

Scope VariableRegistry::getGlobalScope() {
  return Scope::createGlobal("___globals__");
}

void VariableRegistry::fetchFromStack(lua_State* L, int level, lua_State* src) {
  // Register local variables
  std::vector<Variable> variables;
  int index = 1;
  while (const char* name = lua_getlocal(L, level, index++)) {
    variables.emplace_back(createVariable(L, name, level));
    lua_pop(L, 1);
  }
  registerOrUpdateVariables(getLocalScope(src, depth_), std::move(variables));

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
  registerOrUpdateVariables(getUpvalueScope(src, depth_), std::move(upvalues));
}

}  // namespace luau::debugger