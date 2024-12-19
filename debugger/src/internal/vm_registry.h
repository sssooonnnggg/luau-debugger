#pragma once

#include <unordered_map>
#include <vector>

#include <lua.h>

namespace luau::debugger {
class VMRegistry {
 public:
  ~VMRegistry();
  void registerVM(lua_State* L);
  bool isAlive(lua_State* L) const;
  bool isChild(lua_State* L, lua_State* parent) const;
  lua_State* getParent(lua_State* L) const;
  lua_State* getRoot(lua_State* L) const;
  void markAlive(lua_State* L, lua_State* parent);
  void markDead(lua_State* L);

  std::vector<lua_State*> getAncestors(lua_State* L) const;
  std::vector<lua_State*> getThreads() const;

 private:
  std::vector<lua_State*> lua_vms_;

  // Record alive threads with its parent
  std::unordered_map<lua_State*, lua_State*> alive_threads_;
};
}  // namespace luau::debugger
