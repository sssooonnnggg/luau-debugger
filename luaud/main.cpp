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

  auto log_handler = [](std::string_view msg) {
    printf("%s", msg.data());
    printToDebugConsole(msg);
  };
  auto error_handler = [](std::string_view msg) {
    fprintf(stderr, "%s", msg.data());
  };

  luau::Runtime runtime;
  luau::debugger::log::install(log_handler, error_handler);
  luau::debugger::Debugger debugger(true);

  runtime.setErrorHandler(error_handler);
  runtime.installDebugger(&debugger);
  runtime.installLibrary();

  debugger.listen(std::atoi(argv[1]));
  int result = runtime.runFile(argv[2]);
  debugger.stop();
  return result ? 0 : -1;
}
