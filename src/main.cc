#include "app.h"
#include "runtime.h"

int main(int argc, char* argv[]) {
  PosixAppRuntime runtime;
  CpuMonitorApp app(runtime);
  return app.Main(argc, argv);
}
