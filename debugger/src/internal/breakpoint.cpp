#include <lua.h>

#include <internal/breakpoint.h>
#include <internal/log.h>
#include <internal/utils.h>
#include "internal/utils/lua_utils.h"

namespace luau::debugger {

BreakPoint BreakPoint::create(int line) {
  BreakPoint bp;
  bp.line_ = line;
  return bp;
}

int BreakPoint::line() const {
  return line_;
}

int BreakPoint::targetLine() const {
  return target_line_;
}

void BreakPoint::setCondition(std::string condition) {
  condition_ = std::move(condition);
}

const std::string& BreakPoint::condition() const {
  return condition_;
}

BreakPoint::HitResult BreakPoint::hit(lua_State* L) const {
  if (condition_.empty())
    return HitResult::success(true);

  lua_utils::StackGuard guard(L);
  if (!lua_utils::pushBreakEnv(L, 0))
    return HitResult::error("Invalid condition: environment not found");

  auto result = lua_utils::eval(L, condition_, -1);
  if (!result.has_value())
    return HitResult::error("Invalid condition: syntax error");

  if (result.value() != 1)
    return HitResult::error("Invalid condition: must return a boolean value");

  if (lua_type(L, -1) != LUA_TBOOLEAN)
    return HitResult::error("Invalid condition: must return a boolean value");

  return HitResult::success(lua_toboolean(L, -1) != 0);
}

int BreakPoint::enable(lua_State* L, int func_index, bool enable) {
  lua_checkstack(L, 1);
  lua_getref(L, func_index);
  int result = lua_breakpoint(L, -1, line_, enable);
  if (result != -1)
    target_line_ = result;
  lua_pop(L, 1);
  return result;
}

}  // namespace luau::debugger