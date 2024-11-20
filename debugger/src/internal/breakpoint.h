#pragma once

#include <lua.h>

namespace luau::debugger {
class BreakPoint {
 public:
  static BreakPoint create(int line);

  int line() const;
  bool enable(lua_State* L, int func_index, bool enable);

 private:
  int line_ = 0;
};
}  // namespace luau::debugger