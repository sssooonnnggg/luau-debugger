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
  // Disable buffering for stdout and stderr
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);

  if (argc != 3) {
    printf("Usage: %s [DEBUGGER_PORT] [FILE_PATH]\n", argv[0]);
    return -1;
  }

  auto log_handler = [](std::string_view msg) { printf("%s", msg.data()); };
  auto error_handler = [](std::string_view msg) {
    fprintf(stderr, "%s", msg.data());
  };

  luau::debugger::log::install(log_handler, error_handler);
  luau::debugger::Debugger debugger(true);
  debugger.listen(std::atoi(argv[1]));

  luau::Runtime runtime;
  runtime.setErrorHandler(error_handler);
  runtime.installLibrary();
  runtime.installDebugger(&debugger);
  int result = runtime.runFile(argv[2]);

#if defined(MULTIPLY_VM_TEST)
  luau::Runtime runtime2;
  runtime2.setErrorHandler(error_handler);
  runtime2.installLibrary();
  runtime2.installDebugger(&debugger);
  result = runtime2.runFile(argv[2]);
#endif

  debugger.stop();

  return result ? 0 : -1;
}
