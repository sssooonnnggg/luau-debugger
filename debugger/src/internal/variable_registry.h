#pragma once

#include <string_view>
#include <type_traits>
#include <unordered_map>

#include <lua.h>
#include <optional>
#include <vector>

#include "internal/variable.h"

namespace luau::debugger {

struct RegistryKey {
  inline static constexpr auto MASK = 0x7FFFFFFF;
  RegistryKey(std::string_view name) {
    // The key should be limited to (0, 2^31)
    key_ = std::hash<std::string_view>{}(name)&MASK;
    name_ = name;
  }
  template <class T>
    requires(!std::is_same_v<T, char>)
  RegistryKey(const T* ptr) {
    key_ = std::hash<const T*>{}(ptr)&MASK;
  }
  RegistryKey(int key) : key_(key) {}
  bool operator==(const RegistryKey& other) const { return key_ == other.key_; }
  int key_;
  std::string name_;
};
}  // namespace luau::debugger

template <>
struct std::hash<luau::debugger::RegistryKey> {
  std::size_t operator()(const luau::debugger::RegistryKey& key) const {
    return std::hash<int>{}(key.key_);
  }
};

namespace luau::debugger {

class VariableRegistry {
 public:
  Variable createVariable(lua_State* L, std::string_view name);
  void clear();

  using Iter = std::unordered_map<RegistryKey, std::vector<Variable>>::iterator;
  std::pair<Iter, bool> registerVariables(RegistryKey key,
                                          std::vector<Variable> variables);
  bool isRegistered(RegistryKey key) const;
  std::optional<std::reference_wrapper<std::vector<Variable>>> getVariables(
      int reference);

 private:
  std::unordered_map<RegistryKey, std::vector<Variable>> variables_;
};
}  // namespace luau::debugger