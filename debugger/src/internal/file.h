#pragma once

#include <lua.h>
#include <string>
#include <unordered_map>
#include <vector>

#include <internal/breakpoint.h>
#include <internal/utils.h>

namespace luau::debugger {
struct FileRef final {
  FileRef(lua_State* L);
  ~FileRef();

  FileRef(const FileRef&);
  FileRef& operator=(const FileRef&);
  auto operator<=>(const FileRef&) const = default;

  lua_State* L_ = nullptr;
  int file_ref_ = LUA_REFNIL;
  int thread_ref_ = LUA_REFNIL;
};

class File {
 public:
  void setPath(std::string path);
  std::string_view path() const;

  void setBreakPoints(const std::unordered_map<int, BreakPoint>& breakpoints);
  void addRef(FileRef ref);

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
  std::vector<FileRef> refs_;
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