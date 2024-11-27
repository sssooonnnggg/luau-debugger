#pragma once
#include <string>

#include <lua.h>

#include <internal/utils.h>

namespace luau::debugger {
class BreakPoint {
 public:
  static BreakPoint create(int line);

  int line() const;
  bool enable(lua_State* L, int func_index, bool enable);
  void setCondition(std::string condition);
  const std::string& condition() const;

  using HitResult = utils::Result<bool>;
  HitResult hit(lua_State* L) const;

 private:
  std::string condition_;
  int line_ = 0;
};
}  // namespace luau::debugger