#pragma once
#include <nlohmann_json_serializer.h>
#include <concepts>

#include <dap/protocol.h>
#include <dap/typeof.h>

#include <Luau/BytecodeBuilder.h>
#include <Luau/Common.h>
#include <Luau/Compiler.h>
#include <lua.h>

#include <internal/log.h>

namespace luau::debugger::dap_utils {

template <class T>
concept Serializable =
    std::derived_from<T, dap::Request> || std::derived_from<T, dap::Event> ||
    std::derived_from<T, dap::Response>;

template <Serializable T>
inline std::string toString(const T& t) {
  dap::json::NlohmannSerializer s;
  dap::TypeOf<T>::type()->serialize(&s, reinterpret_cast<const void*>(&t));
  return s.dump();
}

}  // namespace luau::debugger::dap_utils