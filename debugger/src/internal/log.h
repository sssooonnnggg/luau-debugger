#pragma once

#include <format>
#include <functional>
#include <string_view>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace luau::debugger::log {
using Logger = std::function<void(std::string_view)>;
inline Logger& info() {
  static Logger logger;
  return logger;
}

inline Logger& error() {
  static Logger logger;
  return logger;
}

inline void debug_break() {
#if defined(_WIN32)
  if (::IsDebuggerPresent())
    ::DebugBreak();
#endif
}
}  // namespace luau::debugger::log

#define DEBUGGER_LOG_INFO(...)                                                \
  log::info()(                                                                \
      std::format("[info][Luau.Debugger][{}:{}] ", __FILE_NAME__, __LINE__) + \
      std::format(__VA_ARGS__) + "\n")

#define DEBUGGER_LOG_ERROR(...)                                      \
  log::error()(std::format("\x1B[31m[error][Luau.Debugger][{}:{}] ", \
                           __FILE_NAME__, __LINE__) +                \
               std::format(__VA_ARGS__) + "\033[0m\n")

#define DEBUGGER_ASSERT(expr)                                                 \
  if (!(expr)) {                                                              \
    auto assert_message =                                                     \
        std::format("\x1B[31m[assert][Luau.Debugger][{}:{}] ", __FILE_NAME__, \
                    __LINE__) +                                               \
        #expr + "\033[0m\n";                                                  \
    log::error()(assert_message);                                             \
    log::debug_break();                                                       \
  }