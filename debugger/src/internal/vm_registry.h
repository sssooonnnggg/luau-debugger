#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include <lua.h>

namespace luau::debugger {

struct ThreadInfo {
  int key_ = 0;
  lua_State* L_ = nullptr;
  std::string name_;
};

class VMRegistry {
 public:
  ~VMRegistry();
  void registerVM(lua_State* L);
  void releaseVM(lua_State* L);

  bool isAlive(lua_State* L) const;
  bool isChild(lua_State* L, lua_State* parent) const;
  lua_State* getParent(lua_State* L) const;
  lua_State* getRoot(lua_State* L) const;
  void markAlive(lua_State* L, lua_State* parent);
  void markDead(lua_State* L);

  std::vector<lua_State*> getAncestors(lua_State* L) const;
  std::vector<std::vector<lua_State*>> getThreadWithAncestors() const;

  std::vector<ThreadInfo> getThreads() const;
  static int getThreadKey(lua_State* L);
  static std::string getThreadName(lua_State* L);
  lua_State* getThread(int key) const;

  void pushStack(lua_State* L);
  void popStack();

 private:
  std::vector<lua_State*> lua_vms_;
  std::unordered_set<lua_State*> alive_threads_;

  std::vector<lua_State*> thread_stack_;
};
}  // namespace luau::debugger
