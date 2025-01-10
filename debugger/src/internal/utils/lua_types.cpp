#include <lua.h>

#include <internal/utils/lua_utils.h>

#include "lua_types.h"

namespace luau::debugger::lua_utils::type {

std::string formatComplexData(lua_State* L, int index) {
  DisableDebugStep _(L);
  lua_checkstack(L, 1);
  const void* ptr = lua_topointer(L, index);
  unsigned long long enc = lua_encodepointer(L, uintptr_t(ptr));
  lua_pushfstring(L, "%s: 0x%016llx", luaL_typename(L, index), enc);
  const char* data = lua_tolstring(L, -1, nullptr);
  auto result = std::format("{}", data);
  lua_pop(L, 1);
  if (lua_utils::callMetaProtected(L, index, "__tostring")) {
    const char* s = lua_tolstring(L, -1, nullptr);
    if (s != nullptr)
      result = std::format("{} ({})", result, s);
    lua_pop(L, 1);
  }
  return result;
}

using ToString = std::string (*)(lua_State*, int);
using GetTypeName = std::string (*)();
struct Processor {
  ToString toString_;
  GetTypeName getTypeName_;
};
using TypeProcessors = std::unordered_map<lua_Type, Processor>;

template <class T>
class With;
template <Type... T>
class With<TypeList<T...>> {
 public:
  static TypeProcessors createProcessors() {
    return {{T::type, Processor{T::toString, T::typeName}}...};
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
}

std::string getTypeName(int type) {
  auto it = processors().find(static_cast<lua_Type>(type));
  return it == processors().end() ? "unknown" : it->second.getTypeName_();
}

}  // namespace luau::debugger::lua_utils::type