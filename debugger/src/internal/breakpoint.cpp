#include <lua.h>

#include <internal/breakpoint.h>
#include <internal/log.h>
#include <internal/utils.h>

namespace luau::debugger {

BreakPoint BreakPoint::create(int line) {
  BreakPoint bp;
  bp.line_ = line;
  return bp;
}

int BreakPoint::line() const {
  return line_;
}

bool BreakPoint::enable(lua_State* L, int func_index, bool enable) {
  lua_checkstack(L, 1);
  lua_getref(L, func_index);
  bool result = lua_breakpoint(L, -1, line_, enable) != -1;
  lua_pop(L, 1);
  return result;
}

}  // namespace luau::debugger