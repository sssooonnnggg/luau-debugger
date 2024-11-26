#pragma once

#include <type_traits>

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

}  // namespace luau::debugger::utils