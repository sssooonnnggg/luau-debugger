#include "internal/variable_registry.h"
#include <string_view>
#include "internal/log.h"

namespace luau::debugger {

Variable VariableRegistry::createVariable(lua_State* L, std::string_view name) {
  return Variable(this, L, name);
}

void VariableRegistry::clear() {
  variables_.clear();
}

std::pair<VariableRegistry::Iter, bool> VariableRegistry::registerVariables(
    RegistryKey key,
    std::vector<Variable> variables) {
  auto result = variables_.try_emplace(key, std::move(variables));
  if (!result.second)
    DEBUGGER_LOG_ERROR("Variable already registered: {}",
                       result.first->first.name_);
  return result;
}

bool VariableRegistry::isRegistered(RegistryKey key) const {
  return variables_.find(key) != variables_.end();
}

std::optional<std::reference_wrapper<std::vector<Variable>>>
VariableRegistry::getVariables(int reference) {
  auto it = variables_.find(RegistryKey(reference));
  if (it == variables_.end()) {
    DEBUGGER_LOG_ERROR("Variable not found: {}", reference);
    return std::nullopt;
  }

  return std::ref(it->second);
}

}  // namespace luau::debugger