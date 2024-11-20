#pragma once

#include <lua.h>
#include <string>
#include <unordered_map>

#include "internal/breakpoint.h"
#include "internal/utils.h"

namespace luau::debugger {
class File {
 public:
  File() = default;
  ~File();

  File(const File&) = delete;
  File(File&& other);

  File& operator=(File&& other);

  static File fromLuaState(lua_State* L, std::string_view path);
  static File fromBreakPoints(std::string_view path,
                              std::unordered_map<int, BreakPoint> breakpoints);
  bool isLoaded() const;

  void addBreakPoint(int line);
  void removeBreakPoint(int line);
  void clearBreakPoints();

  template <class Predicate>
  void removeBreakPointsIf(Predicate pred);

  void syncBreakpoints(const File& other);

 private:
  lua_State* L_ = nullptr;
  int thread_ref = LUA_REFNIL;
  int file_ref_ = LUA_REFNIL;
  std::string path_;
  std::unordered_map<int, BreakPoint> breakpoints_;
};

template <class Predicate>
void File::removeBreakPointsIf(Predicate pred) {
  for (auto it = breakpoints_.begin(); it != breakpoints_.end();) {
    if (pred(it->second)) {
      DEBUGGER_LOG_INFO("Remove breakpoint: {}:{}", path_, it->first);
      if (isLoaded())
        it->second.enable(L_, file_ref_, false);
      it = breakpoints_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace luau::debugger