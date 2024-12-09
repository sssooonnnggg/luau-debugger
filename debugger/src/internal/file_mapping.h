#pragma once

#include <filesystem>
#include <string_view>
#include <utility>

namespace luau::debugger {
class FileMapping {
 public:
  void setRootDirectory(std::string_view root) {
    lua_root_ = std::string(root);
  }
  void setEntryPath(std::string entry) { entry_path_ = std::move(entry); }
  std::string entryPath() const { return entry_path_; }
  bool isEntryPath(std::string_view path) const {
    return entry_path_ == normalize(path);
  }

  std::string normalize(std::string_view path) const {
    if (path.empty())
      return std::string{};
    std::string prefix_removed =
        std::string((path[0] == '@' || path[0] == '=') ? path.substr(1) : path);
    if (std::filesystem::path(prefix_removed).is_relative())
      prefix_removed = lua_root_ + "/" + prefix_removed;
    std::string with_extension = std::filesystem::path(prefix_removed)
                                     .replace_extension(".lua")
                                     .string();
    return std::filesystem::weakly_canonical(with_extension).string();
  }

 public:
  std::string lua_root_;
  std::string entry_path_;
};

}  // namespace luau::debugger