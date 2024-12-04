#pragma once
#include <lua.h>
#include <string>

#include <internal/utils.h>

namespace luau::debugger {
class BreakPoint {
 public:
  static BreakPoint create(int line);

  int line() const;
  int targetLine() const;
  int enable(lua_State* L, int func_index, bool enable);
  void setCondition(std::string condition);
  const std::string& condition() const;

  using HitResult = utils::Result<bool>;
  HitResult hit(lua_State* L) const;

 private:
  std::string condition_;
  int line_ = 0;
  int target_line_ = -1;
};
}  // namespace luau::debugger