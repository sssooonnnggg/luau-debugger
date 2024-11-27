#pragma once

#include <Luau/Common.h>
#include <Luau/Compiler.h>
#include <lua.h>

#include <Luau/BytecodeBuilder.h>
#include <internal/log.h>

namespace luau::debugger::lua_utils {

// env is the index of the environment table
// return value is the number of results on stack for the code to evaluate
// if failed to evaluate, return nullopt and the error message is on the stack
std::optional<int> eval(lua_State* L, const std::string& code, int env);

// convert a lua value at index to a string to display in the debugger
std::string toString(lua_State* L, int index);

// push a new environment table to the stack
bool pushBreakEnv(lua_State* L, int level);

bool setLocal(lua_State* L, int level, const std::string& name, int index);

bool setUpvalue(lua_State* L, int level, const std::string& name, int index);

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

}  // namespace luau::debugger::lua_utils