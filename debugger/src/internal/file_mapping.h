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
  void setFileExtension(std::string_view extension) {
    lua_file_extension_ = std::string(extension);
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
#if !defined(_WIN32)
    std::replace(prefix_removed.begin(), prefix_removed.end(), '\\', '/');
#endif
    if (std::filesystem::path(prefix_removed).is_relative())
      prefix_removed = lua_root_ + "/" + prefix_removed;
    std::filesystem::path full_path(prefix_removed);
    std::string with_extension =
        full_path.has_extension()
            ? full_path.string()
            : full_path.replace_extension(lua_file_extension_).string();
    return std::filesystem::weakly_canonical(with_extension).string();
  }

 public:
  std::string lua_root_;
  std::string lua_file_extension_ = ".lua";
  std::string entry_path_;
};

}  // namespace luau::debugger