#include <cstdio>

#if defined(_WIN32)
#include <windows.h>
#endif

#if defined(__ANDROID__)
#include <android/log.h>
#endif

#include "debugger.h"
#include "luau_runtime.h"

void printToDebugConsole(std::string_view msg) {
#if defined(_WIN32)
  OutputDebugStringA(msg.data());
#endif
}

int main(int argc, const char** argv) {
  // Disable buffering for stdout and stderr
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);

  if (argc != 3) {
    printf("Usage: %s [DEBUGGER_PORT] [FILE_PATH]\n", argv[0]);
    return -1;
  }

  auto log_handler = [](std::string_view msg) {
    printf("%s", msg.data());
#if defined(__ANDROID__)
    __android_log_print(ANDROID_LOG_INFO, "luaud", "%s", msg.data());
#endif
  };
  auto error_handler = [](std::string_view msg) {
    fprintf(stderr, "%s", msg.data());
#if defined(__ANDROID__)
    __android_log_print(ANDROID_LOG_ERROR, "luaud", "%s", msg.data());
#endif
  };

  luau::debugger::log::install(log_handler, error_handler);
  luau::debugger::Debugger debugger(true);
  debugger.listen(std::atoi(argv[1]));

  luau::Runtime runtime;
  runtime.setErrorHandler(error_handler);
  runtime.installLibrary();
  runtime.installDebugger(&debugger);
  int result = runtime.runFile(argv[2]);

#if defined(RELOAD_LUA_FILES_TEST)
  runtime.reset();
  result = runtime.runFile(argv[2]);
#endif

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

#if defined(__ANDROID__)
#include <jni.h>
// export main function as extern "C" and exported for android
extern "C" JNIEXPORT int JNICALL
Java_com_example_test_1luau_1debugger_MainActivity_main(JNIEnv* env,
                                                        jobject thiz,
                                                        jint argc,
                                                        jobjectArray argv) {
  std::vector<jstring> jargs;
  std::vector<const char*> args;
  for (int i = 0; i < argc; i++) {
    jstring arg = (jstring)env->GetObjectArrayElement(argv, i);
    const char* native_string = env->GetStringUTFChars(arg, nullptr);
    jargs.push_back(arg);
    args.push_back(native_string);
  }

  int result = main(argc, args.data());

  for (int i = 0; i < argc; i++)
    env->ReleaseStringUTFChars(jargs[i], args[i]);

  return result;
}
#endif
