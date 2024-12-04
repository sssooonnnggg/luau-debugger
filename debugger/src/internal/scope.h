#pragma once

#include <string>
#include <string_view>
#include <type_traits>

#include <lapi.h>
#include <lstate.h>
#include <lua.h>

namespace luau::debugger {

enum class ScopeType {
  Local,
  UpValue,
  Table,
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

    newRef(other.ref_);
  }
  Scope(Scope&& other) {
    L_ = other.L_;
    key_ = other.key_;
    name_ = std::move(other.name_);
    type_ = other.type_;
    newRef(other.ref_);
  }
  ~Scope() {
    if (type_ == ScopeType::Table && ref_ != LUA_REFNIL && L_ != nullptr)
      lua_unref(L_, ref_);
  }

  static Scope createLocal(std::string_view name) {
    Scope scope;
    scope.createKey(std::hash<std::string_view>{}(name));
    scope.type_ = ScopeType::Local;
    scope.name_ = name;
    return scope;
  }

  static Scope createUpvalue(std::string_view name) {
    Scope scope;
    scope.createKey(std::hash<std::string_view>{}(name));
    scope.type_ = ScopeType::UpValue;
    scope.name_ = name;
    return scope;
  }

  static Scope createTable(lua_State* L, int index = -1) {
    const TValue* t = luaA_toobject(L, index);
    auto* address = hvalue(t);
    return createFromAddress<Table>(L, index, address);
  }

  static Scope createUserData(lua_State* L, int index = -1) {
    const TValue* u = luaA_toobject(L, index);
    auto* address = uvalue(u);
    return createFromAddress<Udata>(L, index, address);
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

  bool isLocal() const { return type_ == ScopeType::Local; }
  bool isUpvalue() const { return type_ == ScopeType::UpValue; }
  bool isTable() const { return type_ == ScopeType::Table; }

  bool pushRef() {
    if (ref_ == LUA_REFNIL || L_ == nullptr)
      return false;

    lua_checkstack(L_, 1);
    lua_getref(L_, ref_);
    return true;
  }

 private:
  template <class T>
  static Scope createFromAddress(lua_State* L, int index, const T* address) {
    Scope scope;
    scope.L_ = L;
    scope.createKey(std::hash<const T*>{}(address));
    scope.type_ = ScopeType::Table;
    scope.ref_ = lua_ref(L, index);
    return scope;
  }

  void createKey(std::size_t hash) { key_ = hash & MASK; }

  void newRef(int ref) {
    if (ref == LUA_REFNIL)
      return;

    lua_checkstack(L_, 1);
    lua_getref(L_, ref);
    ref_ = lua_ref(L_, -1);
    lua_pop(L_, 1);
  }

 private:
  // The key should be limited to (0, 2^31)
  inline static constexpr auto MASK = 0x7FFFFFFF;
  int key_ = 0;
  std::string name_;
  ScopeType type_ = ScopeType::Local;
  lua_State* L_ = nullptr;
  int ref_ = LUA_REFNIL;
};

}  // namespace luau::debugger

template <>
struct std::hash<luau::debugger::Scope> {
  std::size_t operator()(const luau::debugger::Scope& scope) const {
    return std::hash<int>{}(scope.getKey());
  }
};