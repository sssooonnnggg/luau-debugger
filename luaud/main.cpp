#include <cstdio>

#if defined(_WIN32)
#include <windows.h>
#endif

#include "debugger.h"
#include "luau_runtime.h"

void printToDebugConsole(std::string_view msg) {
#if defined(_WIN32)
  OutputDebugStringA(msg.data());
#endif
}

int main(int argc, char** argv) {
  if (argc == 1) {
    printf("Usage: %s [FILE_PATH]\n", argv[0]);
    return -1;
  }

  luau::Runtime runtime;

  luau::debugger::log::install(
      [](std::string_view msg) {
        printf("%s", msg.data());
        printToDebugConsole(msg);
      },
      [](std::string_view msg) {
        fprintf(stderr, "%s", msg.data());
        printToDebugConsole(msg);
      });

  luau::debugger::Debugger debugger(true);
  runtime.installDebugger(&debugger);
  runtime.installLibrary();

  debugger.listen(58000);

  return runtime.runFile(argv[1]) ? 0 : -1;
}
