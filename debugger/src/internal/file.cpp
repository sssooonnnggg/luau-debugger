#include <lua.h>
#include <utility>

#include <internal/breakpoint.h>
#include <internal/file.h>
#include <internal/utils.h>

namespace luau::debugger {

File File::fromLuaState(lua_State* L, std::string_view path) {
  File file;
  file.L_ = L;
  lua_checkstack(L, 1);
  lua_pushthread(L);
  file.thread_ref = lua_ref(L, -1);
  lua_pop(L, 1);
  file.file_ref_ = lua_ref(L, -1);
  file.path_ = path;
  return file;
}

File File::fromBreakPoints(std::string_view path,
                           std::unordered_map<int, BreakPoint> breakpoints) {
  File file;
  file.path_ = path;
  file.breakpoints_ = breakpoints;
  return file;
}

File::~File() {
  if (isLoaded()) {
    lua_unref(L_, file_ref_);
    lua_unref(L_, thread_ref);
  }
}

File::File(File&& other) {
  L_ = other.L_;
  file_ref_ = other.file_ref_;
  path_ = std::move(other.path_);
  breakpoints_ = std::move(other.breakpoints_);

  other.L_ = nullptr;
  other.file_ref_ = LUA_REFNIL;
}

File& File::operator=(File&& other) {
  if (this != &other) {
    if (isLoaded())
      lua_unref(L_, file_ref_);

    L_ = other.L_;
    file_ref_ = other.file_ref_;
    path_ = std::move(other.path_);
    breakpoints_ = std::move(other.breakpoints_);

    other.L_ = nullptr;
    other.file_ref_ = LUA_REFNIL;
  }
  return *this;
}

bool File::isLoaded() const {
  return L_ != nullptr;
}

void File::addBreakPoint(int line) {
  auto it = breakpoints_.find(line);
  if (it == breakpoints_.end()) {
    DEBUGGER_LOG_INFO("Add breakpoint: {}:{}", path_, line);
    auto bp = BreakPoint::create(line);
    if (isLoaded())
      bp.enable(L_, file_ref_, true);
    breakpoints_.emplace(line, std::move(bp));
  } else {
    DEBUGGER_LOG_INFO("Breakpoint already exists, ignore: {}:{}", path_, line);
  }
}

void File::removeBreakPoint(int line) {
  auto it = breakpoints_.find(line);
  if (it != breakpoints_.end()) {
    DEBUGGER_LOG_INFO("Remove breakpoint: {}:{}", path_, line);
    if (isLoaded())
      it->second.enable(L_, file_ref_, false);
    breakpoints_.erase(it);
  }
}

void File::clearBreakPoints() {
  DEBUGGER_LOG_INFO("Clear all breakpoints: {}", path_);
  for (auto& [line, bp] : breakpoints_) {
    if (isLoaded())
      bp.enable(L_, file_ref_, false);
  }
  breakpoints_.clear();
}

void File::syncBreakpoints(const File& other) {
  breakpoints_ = other.breakpoints_;

  if (isLoaded())
    for (auto& [line, bp] : breakpoints_)
      bp.enable(L_, file_ref_, true);
}

}  // namespace luau::debugger