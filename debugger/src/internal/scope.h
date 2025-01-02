#pragma once

#include <string>
#include <string_view>
#include <type_traits>
#include "utils/dap_utils.h"

#include <lapi.h>
#include <lstate.h>
#include <lua.h>

namespace luau::debugger {

enum class ScopeType {
  Local,
  UpValue,
  Global,
  Table,
  UserData,
  Unknown,
};

class Scope final {
 public:
  Scope() = default;
  Scope(const Scope& other) {
    L_ = other.L_;
    key_ = other.key_;
    name_ = other.name_;
    type_ = other.type_;
    loaded_ = other.loaded_;
    level_ = other.level_;

    newRef(other.ref_);
  }
  Scope(Scope&& other) {
    L_ = other.L_;
    key_ = other.key_;
    name_ = std::move(other.name_);
    type_ = other.type_;
    loaded_ = other.loaded_;
    level_ = other.level_;

    newRef(other.ref_);
  }
  ~Scope() {
    if (type_ == ScopeType::Table && ref_ != LUA_REFNIL && L_ != nullptr)
      lua_unref(L_, ref_);
  }

  static Scope createLocal(std::string_view name) {
    return createWithType(name, ScopeType::Local);
  }

  static Scope createUpvalue(std::string_view name) {
    return createWithType(name, ScopeType::UpValue);
  }

  static Scope createGlobal(std::string_view name) {
    return createWithType(name, ScopeType::Global);
  }

  static Scope createTable(lua_State* L, int index = -1) {
    const TValue* t = luaA_toobject(L, index);
    auto* address = hvalue(t);
    auto scope = createFromAddress<Table>(L, index, address);
    scope.type_ = ScopeType::Table;
    return scope;
  }

  static Scope createUserData(lua_State* L, int index = -1) {
    const TValue* u = luaA_toobject(L, index);
    auto* address = uvalue(u);
    auto scope = createFromAddress<Udata>(L, index, address);
    scope.type_ = ScopeType::UserData;
    return scope;
  }

  explicit Scope(int key) : key_(key) { type_ = ScopeType::Unknown; }

  bool operator==(const Scope& other) const { return key_ == other.key_; }
  Scope& operator=(const Scope& other) {
    L_ = other.L_;
    key_ = other.key_;
    name_ = other.name_;
    type_ = other.type_;
    newRef(other.ref_);
    return *this;
  }

  int getKey() const { return key_; }
  std::string_view getName() const { return name_; }
  void setName(std::string name) { name_ = std::move(name); }
  int getLevel() const { return level_; }
  void setLevel(int level) { level_ = level; }
  lua_State* getLuaState() const { return L_; }

  bool isLocal() const { return type_ == ScopeType::Local; }
  bool isUpvalue() const { return type_ == ScopeType::UpValue; }
  bool isTable() const { return type_ == ScopeType::Table; }
  bool isUserData() const { return type_ == ScopeType::UserData; }
  bool isLoaded() const {
    if (!isTable() && !isUserData())
      return true;
    return loaded_;
  }
  bool markLoaded() const { return loaded_ = true; }
  bool markUnloaded() const { return loaded_ = false; }

  bool pushRef() const {
    if (ref_ == LUA_REFNIL || L_ == nullptr)
      return false;

    lua_checkstack(L_, 1);
    lua_getref(L_, ref_);
    return true;
  }

 private:
  static Scope createWithType(std::string_view name, ScopeType type) {
    Scope scope;
    scope.createKey(std::hash<std::string_view>{}(name));
    scope.type_ = type;
    scope.name_ = name;
    return scope;
  }

  template <class T>
  static Scope createFromAddress(lua_State* L, int index, const T* address) {
    Scope scope;
    scope.L_ = L;
    scope.createKey(std::hash<const T*>{}(address));
    scope.ref_ = lua_ref(L, index);
    return scope;
  }

  void createKey(std::size_t hash) { key_ = dap_utils::clamp(hash); }

  void newRef(int ref) {
    if (ref == LUA_REFNIL)
      return;

    lua_checkstack(L_, 1);
    lua_getref(L_, ref);
    ref_ = lua_ref(L_, -1);
    lua_pop(L_, 1);
  }

 private:
  int key_ = 0;
  std::string name_;
  ScopeType type_ = ScopeType::Local;
  lua_State* L_ = nullptr;
  int ref_ = LUA_REFNIL;
  mutable bool loaded_ = false;
  int level_ = 0;
};

}  // namespace luau::debugger

template <>
struct std::hash<luau::debugger::Scope> {
  std::size_t operator()(const luau::debugger::Scope& scope) const {
    return std::hash<int>{}(scope.getKey());
  }
};