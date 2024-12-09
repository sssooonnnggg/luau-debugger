#pragma once

#include <filesystem>
#include <type_traits>
#include <variant>

#include <internal/utils/dap_utils.h>
#include <internal/utils/lua_utils.h>

namespace luau::debugger::utils {

template <class T, class U>
  requires std::is_rvalue_reference_v<T&&> && std::is_rvalue_reference_v<T&&>
class RAII {
 public:
  RAII(T&& setup, U&& cleanup)
      : setup_(std::move(setup)), cleanup_(std::move(cleanup)) {
    setup_();
  }
  ~RAII() { cleanup_(); }

 private:
  T setup_;
  U cleanup_;
};

template <class T, class U>
  requires std::is_rvalue_reference_v<T&&> && std::is_rvalue_reference_v<T&&>
inline decltype(auto) makeRAII(T&& setup, U&& cleanup) {
  return RAII(std::move(setup), std::move(cleanup));
}

template <class T>
  requires(!std::is_reference_v<T>)
class Result {
 public:
  static Result error(std::string_view message) {
    Result result;
    result.data_ = std::string(message);
    return result;
  }
  static Result success(T&& value) {
    Result result;
    result.data_ = std::move(value);
    return result;
  }
  static Result success(const T& value) {
    Result result;
    result.data_ = value;
    return result;
  }

  bool isOk() const { return std::holds_alternative<T>(data_); }
  bool isError() const { return std::holds_alternative<std::string>(data_); }
  T& value() { return std::get<T>(data_); }
  const T& value() const { return std::get<T>(data_); }
  std::string_view error() const { return std::get<std::string>(data_); }

 private:
  std::variant<T, std::string> data_;
};

}  // namespace luau::debugger::utils