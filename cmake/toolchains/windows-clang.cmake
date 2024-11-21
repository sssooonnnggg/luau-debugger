set(LLVM_ROOT ${CMAKE_SOURCE_DIR}/../../LLVM)
set(CMAKE_CXX_COMPILER ${LLVM_ROOT}/bin/clang++.exe)
set(CMAKE_C_COMPILER ${LLVM_ROOT}/bin/clang.exe)
set(CMAKE_RC_COMPILER ${LLVM_ROOT}/bin/llvm-rc.exe)