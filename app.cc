#include "app.h"

#include <cstdio>

int CpuMonitorApp::Main(int argc, char* argv[]) {
  const ParseResult parse_result = ParseArguments(argc, argv);
  if (!parse_result.ok) {
    std::fprintf(stderr, "%s\n%s", parse_result.message.c_str(), UsageMessage());
    return 1;
  }

  if (parse_result.show_help) {
    std::fputs(UsageMessage(), stdout);
    return 0;
  }

  std::string error_message;
  if (!Initialize(parse_result.config, &error_message)) {
    std::fprintf(stderr, "Init failed: %s\n", error_message.c_str());
    return 1;
  }

  const int exit_code = Run(&error_message);
  if (exit_code != 0 && !error_message.empty()) {
    std::fprintf(stderr, "Run failed: %s\n", error_message.c_str());
  }
  return exit_code;
}

bool CpuMonitorApp::Initialize(const AppConfig& config,
                               std::string* error_message) {
  config_ = config;
  state_ = AppState::kInit;

  if (error_message == nullptr) {
    return false;
  }

  state_ = AppState::kRun;
  return true;
}

int CpuMonitorApp::Run(std::string* error_message) {
  if (state_ != AppState::kRun) {
    if (error_message != nullptr) {
      *error_message = "Application entered Run before Init completed.";
    }
    return 1;
  }

  return 0;
}
