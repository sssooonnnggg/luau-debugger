#pragma once

#include <fstream>
#include <optional>
#include <string>

namespace file_utils {
inline std::optional<std::string> readFile(const std::string& name) {
  std::ifstream file(name, std::ios::binary);
  if (!file)
    return std::nullopt;

  std::string source((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
  return source;
}
}  // namespace file_utils