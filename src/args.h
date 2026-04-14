#ifndef ARGS_H_
#define ARGS_H_

#include <optional>
#include <string>

struct AppConfig {
  std::optional<unsigned int> interval_seconds;
  std::optional<std::string> output_path;
};

struct ParseResult {
  bool ok = false;
  bool show_help = false;
  AppConfig config;
  std::string message;
};

ParseResult ParseArguments(int argc, char* argv[]);
const char* UsageMessage();

#endif  // ARGS_H_
