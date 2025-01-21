#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include <lobject.h>
#include <lua.h>

#include <internal/breakpoint.h>
#include <internal/utils.h>

namespace luau::debugger {

// Reference to a loaded Lua file
struct LuaFileRef final {
  LuaFileRef(lua_State* L);
  ~LuaFileRef();

  LuaFileRef(const LuaFileRef& other);
  LuaFileRef& operator=(const LuaFileRef& other);
  bool operator==(const LuaFileRef& other) const;

  void release();
  void copyFrom(const LuaFileRef& other);

  lua_State* L_ = nullptr;
  Closure* func_ = nullptr;
  int file_ref_ = LUA_REFNIL;
  int thread_ref_ = LUA_REFNIL;
};

class File {
 public:
  void setPath(std::string path);
  std::string_view path() const;

  void setBreakPoints(const std::unordered_map<int, BreakPoint>& breakpoints);
  void addRef(LuaFileRef ref);
  void removeRef(lua_State* L);

  void addBreakPoint(const BreakPoint& bp);
  void addBreakPoint(int line);
  void clearBreakPoints();

  template <class Predicate>
  void removeBreakPointsIf(Predicate pred);

  BreakPoint* findBreakPoint(int line);

 private:
  void enableBreakPoint(BreakPoint& bp, bool enable);

 private:
  std::string path_;
  std::unordered_map<int, BreakPoint> breakpoints_;
  std::vector<LuaFileRef> refs_;
};

template <class Predicate>
void File::removeBreakPointsIf(Predicate pred) {
  for (auto it = breakpoints_.begin(); it != breakpoints_.end();) {
    if (pred(it->second)) {
      DEBUGGER_LOG_INFO("Remove breakpoint: {}:{}", path_, it->first);
      enableBreakPoint(it->second, false);
      it = breakpoints_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace luau::debugger