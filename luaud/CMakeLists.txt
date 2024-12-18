# luaud is a luau runtime combined with a luau debugger

if(ANDROID)
  add_library(luaud SHARED)
  target_link_libraries(luaud PRIVATE android log)
else()
  add_executable(luaud)
endif()

target_sources(luaud
  PRIVATE
  main.cpp
  luau_runtime.cpp
)
target_link_libraries(luaud PRIVATE Luau.VM Luau.Compiler Luau.CLI.lib Luau.Config Luau.Debugger)
target_include_directories(
  luaud
  PRIVATE
  ${LUAU_ROOT}/VM/include
  ${LUAU_ROOT}/VM/src
  ${LUAU_ROOT}/Compiler/include
  ${LUAU_ROOT}/CLI
  ${LUAU_ROOT}/Config/include
  ${CMAKE_SOURCE_DIR}/debugger/include
  ${CMAKE_CURRENT_SOURCE_DIR}
)