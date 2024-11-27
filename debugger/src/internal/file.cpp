#include <lua.h>
#include <unordered_set>
#include <utility>

#include <internal/breakpoint.h>
#include <internal/file.h>
#include <internal/utils.h>

namespace luau::debugger {

FileRef::FileRef(lua_State* L) {
  L_ = L;
  lua_checkstack(L, 1);
  lua_pushthread(L);
  thread_ref_ = lua_ref(L, -1);
  lua_pop(L, 1);
  file_ref_ = lua_ref(L, -1);
}

FileRef::~FileRef() {
  lua_unref(L_, file_ref_);
  lua_unref(L_, thread_ref_);
}

FileRef::FileRef(const FileRef& other) {
  L_ = other.L_;
  lua_checkstack(L_, 1);
  lua_getref(L_, other.thread_ref_);
  thread_ref_ = lua_ref(L_, -1);
  lua_pop(L_, 1);
  lua_getref(L_, other.file_ref_);
  file_ref_ = lua_ref(L_, -1);
  lua_pop(L_, 1);
}

void File::setPath(std::string path) {
  path_ = std::move(path);
}

std::string_view File::path() const {
  return path_;
}

void File::setBreakPoints(
    const std::unordered_map<int, BreakPoint>& breakpoints) {
  std::unordered_set<int> settled;
  for (const auto& [_, bp] : breakpoints) {
    addBreakPoint(bp);
    settled.insert(bp.line());
  }
  removeBreakPointsIf([&settled](const BreakPoint& bp) {
    return settled.find(bp.line()) == settled.end();
  });
}

void File::addRef(FileRef ref) {
  for (auto& [_, bp] : breakpoints_)
    bp.enable(ref.L_, ref.file_ref_, true);
  refs_.emplace_back(std::move(ref));
}

void File::enableBreakPoint(BreakPoint& bp, bool enable) {
  for (const auto& ref : refs_)
    bp.enable(ref.L_, ref.file_ref_, enable);
}

void File::addBreakPoint(int line) {
  addBreakPoint(BreakPoint::create(line));
}

void File::addBreakPoint(const BreakPoint& bp) {
  auto it = breakpoints_.find(bp.line());
  if (it == breakpoints_.end()) {
    DEBUGGER_LOG_INFO("Add breakpoint: {}:{}", path_, bp.line());
    auto inserted = breakpoints_.emplace(bp.line(), bp).first;
    enableBreakPoint(inserted->second, true);
  } else {
    it->second = bp;
    DEBUGGER_LOG_INFO("Breakpoint already exists, update it: {}:{}", path_,
                      bp.line());
  }
}

void File::clearBreakPoints() {
  DEBUGGER_LOG_INFO("Clear all breakpoints: {}", path_);
  for (auto& [line, bp] : breakpoints_)
    enableBreakPoint(bp, false);
  breakpoints_.clear();
}

BreakPoint* File::findBreakPoint(int line) {
  auto it = breakpoints_.find(line);
  if (it == breakpoints_.end())
    return nullptr;
  return &it->second;
}

}  // namespace luau::debugger