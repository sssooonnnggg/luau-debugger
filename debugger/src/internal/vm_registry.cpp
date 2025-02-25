#include <format>

#include <lua.h>

#include <internal/utils.h>

#include "vm_registry.h"

namespace luau::debugger {

VMRegistry::~VMRegistry() {
  for (auto L : lua_vms_)
    lua_setthreaddata(L, nullptr);
}

void VMRegistry::registerVM(lua_State* L) {
  lua_vms_.push_back(L);
  markAlive(L, nullptr);
}

void VMRegistry::releaseVM(lua_State* L) {
  lua_vms_.erase(std::remove(lua_vms_.begin(), lua_vms_.end(), L),
                 lua_vms_.end());
  for (auto it = alive_threads_.begin(); it != alive_threads_.end();) {
    if (lua_mainthread(*it) == L)
      it = alive_threads_.erase(it);
    else
      ++it;
  }
}

bool VMRegistry::isAlive(lua_State* L) const {
  return alive_threads_.find(L) != alive_threads_.end();
}

lua_State* VMRegistry::getParent(lua_State* L) const {
  // FIXME: fix with its real parent
  return nullptr;
}

lua_State* VMRegistry::getRoot(lua_State* L) const {
  lua_State* current = L;
  while (auto* p = getParent(current))
    current = p;
  return current;
}

bool VMRegistry::isChild(lua_State* L, lua_State* parent) const {
  lua_State* current = L;
  while (auto* p = getParent(current)) {
    if (p == parent)
      return true;
    else
      current = p;
  }
  return false;
}

void VMRegistry::markAlive(lua_State* L, lua_State* _) {
  alive_threads_.insert(L);
}

void VMRegistry::markDead(lua_State* L) {
  alive_threads_.erase(L);
}

std::vector<lua_State*> VMRegistry::getAncestors(lua_State* L) const {
  std::vector<lua_State*> ancestors;
  while (L != nullptr) {
    ancestors.push_back(L);
    L = getParent(L);
  }
  return ancestors;
}

std::vector<std::vector<lua_State*>> VMRegistry::getThreadWithAncestors()
    const {
  std::vector<std::vector<lua_State*>> threads;
  for (auto state : alive_threads_)
    threads.push_back(getAncestors(state));
  return threads;
}

std::vector<ThreadInfo> VMRegistry::getThreads() const {
  std::vector<ThreadInfo> threads;
  for (auto state : alive_threads_)
    threads.emplace_back(
        ThreadInfo{getThreadKey(state), state, getThreadName(state)});

  return threads;
}

int VMRegistry::getThreadKey(lua_State* L) {
  return dap_utils::clamp(std::hash<lua_State*>{}(L));
}

std::string VMRegistry::getThreadName(lua_State* L) {
  return std::format("Thread ({})", static_cast<void*>(L));
}

lua_State* VMRegistry::getThread(int key) const {
  for (auto state : alive_threads_) {
    if (getThreadKey(state) == key)
      return state;
  }
  return nullptr;
}

}  // namespace luau::debugger