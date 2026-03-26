#ifndef APP_H_
#define APP_H_

#include <string>

#include "args.h"

enum class AppState {
  kInit,
  kRun,
};

class CpuMonitorApp {
 public:
  int Main(int argc, char* argv[]);

 private:
  bool Initialize(const AppConfig& config, std::string* error_message);
  int Run(std::string* error_message);

  AppState state_ = AppState::kInit;
  AppConfig config_;
};

#endif  // APP_H_
