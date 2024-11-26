#include <format>
#include <string>

#include <Luau/Common.h>
#include <Luau/Compiler.h>
#include <Require.h>
#include <lua.h>
#include <lualib.h>

#include <debugger.h>
#include <file_utils.h>

#include "luau_runtime.h"

namespace {
static Luau::CompileOptions copts() {
  Luau::CompileOptions result = {};
  result.optimizationLevel = 1;

  // NOTICE: debugLevel should be set to 2 to enable full debug info
  result.debugLevel = 2;
  result.typeInfoLevel = 1;
  result.coverageLevel = 0;

  return result;
}

static int lua_loadstring(lua_State* L) {
  size_t l = 0;
  const char* s = luaL_checklstring(L, 1, &l);
  const char* chunkname = luaL_optstring(L, 2, s);

  lua_setsafeenv(L, LUA_ENVIRONINDEX, false);

  std::string bytecode = Luau::compile(std::string(s, l), copts());
  if (luau_load(L, chunkname, bytecode.data(), bytecode.size(), 0) == 0)
    return 1;

  lua_pushnil(L);
  lua_insert(L, -2);  // put before error message
  return 2;           // return nil plus error message
}

static int finishrequire(lua_State* L) {
  if (lua_isstring(L, -1))
    lua_error(L);

  return 1;
}

static int lua_require(lua_State* L) {
  std::string name = luaL_checkstring(L, 1);

  RequireResolver::ResolvedRequire resolved_require =
      RequireResolver::resolveRequire(L, std::move(name));

  if (resolved_require.status == RequireResolver::ModuleStatus::Cached)
    return finishrequire(L);
  else if (resolved_require.status == RequireResolver::ModuleStatus::Ambiguous)
    luaL_errorL(L, "require path could not be resolved to a unique file");
  else if (resolved_require.status == RequireResolver::ModuleStatus::NotFound)
    luaL_errorL(L, "error requiring module");

  lua_State* GL = lua_mainthread(L);
  lua_State* ML = lua_newthread(GL);
  lua_xmove(GL, L, 1);

  // now we can compile & run module on the new thread
  std::string bytecode = Luau::compile(resolved_require.sourceCode, copts());
  if (luau_load(ML, resolved_require.chunkName.c_str(), bytecode.data(),
                bytecode.size(), 0) == 0) {
    // NOTICE: Call debugger when file is loaded
    auto* debugger =
        reinterpret_cast<luau::debugger::Debugger*>(lua_getthreaddata(GL));
    debugger->onLuaFileLoaded(ML, resolved_require.absolutePath, false);

    int status = lua_resume(ML, L, 0);

    if (status == 0) {
      if (lua_gettop(ML) == 0)
        lua_pushstring(ML, "module must return a value");
      else if (!lua_istable(ML, -1) && !lua_isfunction(ML, -1))
        lua_pushstring(ML, "module must return a table or function");
    } else if (status == LUA_YIELD) {
      lua_pushstring(ML, "module can not yield");
    } else if (!lua_isstring(ML, -1)) {
      lua_pushstring(ML, "unknown error while running module");
    }
  }

  lua_xmove(ML, L, 1);
  lua_pushvalue(L, -1);
  lua_setfield(L, -4, resolved_require.absolutePath.c_str());

  // L stack: _MODULES ML result
  return finishrequire(L);
}

static int lua_collectgarbage(lua_State* L) {
  const char* option = luaL_optstring(L, 1, "collect");

  if (strcmp(option, "collect") == 0) {
    lua_gc(L, LUA_GCCOLLECT, 0);
    return 0;
  }

  if (strcmp(option, "count") == 0) {
    int c = lua_gc(L, LUA_GCCOUNT, 0);
    lua_pushnumber(L, c);
    return 1;
  }

  luaL_error(L, "collectgarbage must be called with 'count' or 'collect'");
}
}  // namespace

namespace luau {

static Runtime* runtime_ = nullptr;

Runtime::Runtime() {
  Luau::assertHandler() = [](const char* expr, const char* file, int line,
                             const char* function) {
    if (runtime_)
      runtime_->onError(
          std::format("{}({}): ASSERTION FAILED: {}\n", file, line, expr),
          nullptr);
    return 1;
  };
  runtime_ = this;
  vm_ = luaL_newstate();
}

Runtime::~Runtime() {
  lua_close(vm_);
  runtime_ = nullptr;
}

void Runtime::installDebugger(debugger::Debugger* debugger) {
  debugger_ = debugger;
  debugger->initialize(vm_);
}

void Runtime::installLibrary() {
  luaL_openlibs(vm_);

  static const luaL_Reg funcs[] = {
      {"loadstring", lua_loadstring},
      {"require", lua_require},
      {"collectgarbage", lua_collectgarbage},
      {NULL, NULL},
  };

  lua_pushvalue(vm_, LUA_GLOBALSINDEX);
  luaL_register(vm_, NULL, funcs);
  lua_pop(vm_, 1);

  luaL_sandbox(vm_);
}

bool Runtime::runFile(const char* name) {
  std::optional<std::string> source = file_utils::readFile(name);
  if (!source) {
    onError(std::format("Error opening {}\n", name), nullptr);
    return false;
  }

  lua_State* L = lua_newthread(vm_);

  std::string chunkname = "=" + std::string(name);

  std::string bytecode = Luau::compile(*source, copts());
  int status = 0;

  if (luau_load(L, chunkname.c_str(), bytecode.data(), bytecode.size(), 0) ==
      0) {
    // NOTICE: Call debugger when file is loaded
    debugger_->onLuaFileLoaded(L, name, true);
    status = lua_resume(L, NULL, 0);
  } else {
    status = LUA_ERRSYNTAX;
  }

  if (status != 0) {
    std::string msg;

    if (status == LUA_YIELD)
      msg = "thread yielded unexpectedly";
    else if (const char* str = lua_tostring(L, -1))
      msg = str;

    msg += "\nstacktrace:\n";
    msg += lua_debugtrace(L);

    onError(msg, L);
  }

  lua_pop(vm_, 1);
  return status == 0;
}

void Runtime::setErrorHandler(std::function<void(std::string_view)> handler) {
  errorHandler_ = handler;
}

void Runtime::onError(std::string_view msg, lua_State* L) {
  auto with_color = std::format("\x1B[31m{}\033[0m\n", msg);
  if (errorHandler_)
    errorHandler_(with_color);
  if (debugger_)
    debugger_->onError(with_color, L);
}

}  // namespace luau