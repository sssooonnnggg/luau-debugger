#include <lua.h>
#include <unordered_map>

#include <internal/utils/lua_types.h>

#include "lua_types.h"

namespace luau::debugger::lua_utils::type {

using ToString = std::string (*)(lua_State*, int);
struct Processor {
  ToString toString_;
};
using TypeProcessors = std::unordered_map<lua_Type, Processor>;

template <class T>
class With;
template <Type... T>
class With<TypeList<T...>> {
 public:
  static TypeProcessors createProcessors() {
    return {{T::type, Processor{T::toString}}...};
  }
};

TypeProcessors& processors() {
  static auto processors = With<RegisteredTypes>::createProcessors();
  return processors;
}

std::string toString(lua_State* L, int index) {
  lua_Type type = static_cast<lua_Type>(lua_type(L, index));
  auto it = processors().find(type);
  return it == processors().end() ? "unknown" : it->second.toString_(L, index);
}  // namespace type

}  // namespace luau::debugger::lua_utils::type