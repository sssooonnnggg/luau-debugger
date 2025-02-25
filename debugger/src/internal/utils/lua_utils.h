#pragma once

#include <Luau/BytecodeBuilder.h>
#include <Luau/Common.h>
#include <Luau/Compiler.h>
#include <lobject.h>
#include <lua.h>

#include <internal/log.h>
#include <internal/utils/lua_types.h>

namespace luau::debugger::lua_utils {

// env is the index of the environment table
// return value is the number of results on stack for the code to evaluate
// if failed to evaluate, return nullopt and the error message is on the stack
std::optional<int> eval(lua_State* L, const std::string& code, int env);

// push a new environment table to the stack
bool pushBreakEnv(lua_State* L, int level);

bool setLocal(lua_State* L, int level, const std::string& name, int index);

bool setUpvalue(lua_State* L, int level, const std::string& name, int index);

Closure* getLuaFunction(lua_State* L, int index);
Closure* getCFunction(lua_State* L, int index);

// Replace the function with the given name in the global table
// If the function does not exist, create a new one and return false
bool replaceOrCreateFunction(lua_State* L,
                             const std::string& name,
                             lua_CFunction func);

bool replaceOrCreateFunction(lua_State* L,
                             const std::string& library_name,
                             const std::string& name,
                             lua_CFunction func);

int callMetaProtected(lua_State* L, int obj, const char* event);

class StackGuard {
 public:
  StackGuard(lua_State* L, int capacity = 5) : L_(L), top_(lua_gettop(L)) {
    lua_checkstack(L, capacity);
  }
  ~StackGuard() { lua_settop(L_, top_); }

 private:
  lua_State* L_;
  int top_;
};

class ReadOnlyGuard {
 public:
  ReadOnlyGuard(lua_State* L, int index) : L_(L), index_(index) {
    enable_ = lua_getreadonly(L, index);
    lua_setreadonly(L, index, false);
  }
  ~ReadOnlyGuard() { lua_setreadonly(L_, index_, enable_); }

 private:
  lua_State* L_;
  int index_;
  bool enable_;
};

class DisableDebugStep {
 public:
  DisableDebugStep(lua_State* L) {
    main_vm_ = lua_mainthread(L);
    callbacks_ = lua_callbacks(main_vm_);
    old_debug_step_ = callbacks_->debugstep;
    callbacks_->debugstep = nullptr;
  }
  ~DisableDebugStep() { callbacks_->debugstep = old_debug_step_; }

 private:
  lua_State* main_vm_;
  lua_Callbacks* callbacks_;
  using DebugStep = void (*)(lua_State*, lua_Debug*);
  DebugStep old_debug_step_;
};

}  // namespace luau::debugger::lua_utils