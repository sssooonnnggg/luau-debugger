cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(luau_debugger)

if(NOT DEFINED LUAU_ROOT)
  set(LUAU_ROOT ${CMAKE_SOURCE_DIR}/../luau)
endif()

if(NOT DEFINED CPP_DAP_ROOT)
  set(CPP_DAP_ROOT ${CMAKE_SOURCE_DIR}/../cppdap)
endif()

add_subdirectory(${LUAU_ROOT} ${CMAKE_BINARY_DIR}/luau)
add_subdirectory(${CPP_DAP_ROOT} ${CMAKE_BINARY_DIR}/cppdap)

add_subdirectory(debugger)
add_subdirectory(luaud)