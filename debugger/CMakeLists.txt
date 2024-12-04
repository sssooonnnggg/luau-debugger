set(library_name Luau.Debugger)
add_library(${library_name} STATIC)

target_sources(${library_name}
  PRIVATE
  src/debugger.cpp
  src/internal/breakpoint.cpp
  src/internal/file.cpp
  src/internal/debug_bridge.cpp
  src/internal/lua_statics.cpp
  src/internal/variable.cpp
  src/internal/variable_registry.cpp
  src/internal/vm_registry.cpp
  src/internal/utils/lua_utils.cpp
  src/internal/utils/lua_types.cpp
)

target_include_directories(${library_name} PRIVATE
  include
  src
)

target_compile_definitions(cppdap PUBLIC _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)

target_link_libraries(${library_name} PRIVATE cppdap)
target_include_directories(${library_name} PRIVATE
  ${CPP_DAP_ROOT}/include
  ${CPP_DAP_ROOT}/src
  ${CPP_DAP_ROOT}/third_party/json/include
)
target_compile_features(${library_name} PRIVATE cxx_std_20)
target_compile_definitions(${library_name} PUBLIC _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)

target_link_libraries(${library_name} PUBLIC Luau.VM Luau.Compiler)
target_include_directories(${library_name} PRIVATE
  ${LUAU_ROOT}/VM/src
)
