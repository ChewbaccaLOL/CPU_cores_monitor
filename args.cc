#include "args.h"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <string_view>

namespace {

bool ParseUnsignedArgument(const char* value, unsigned int* output) {
  if (value == nullptr || *value == '\0') {
    return false;
  }

  errno = 0;
  char* end = nullptr;
  const unsigned long parsed = std::strtoul(value, &end, 10);
  if (errno != 0 || end == value || *end != '\0' || parsed == 0 ||
      parsed > static_cast<unsigned long>(UINT_MAX)) {
    return false;
  }

  *output = static_cast<unsigned int>(parsed);
  return true;
}

}  // namespace

ParseResult ParseArguments(int argc, char* argv[]) {
  ParseResult result;

  for (int index = 1; index < argc; ++index) {
    const std::string_view argument(argv[index]);

    if (argument == "--help" || argument == "-h") {
      result.ok = true;
      result.show_help = true;
      return result;
    }

    if (argument == "--interval-sec") {
      if (index + 1 >= argc) {
        result.message = "Missing value for --interval-sec.";
        return result;
      }

      unsigned int interval = 0;
      if (!ParseUnsignedArgument(argv[index + 1], &interval)) {
        result.message = "Invalid value for --interval-sec.";
        return result;
      }

      result.config.interval_seconds = interval;
      ++index;
      continue;
    }

    if (argument == "--output") {
      if (index + 1 >= argc) {
        result.message = "Missing value for --output.";
        return result;
      }

      result.config.output_path = std::string(argv[index + 1]);
      ++index;
      continue;
    }

    result.message = "Unknown argument: " + std::string(argument);
    return result;
  }

  const bool has_interval = result.config.interval_seconds.has_value();
  const bool has_output = result.config.output_path.has_value();
  if (has_interval != has_output) {
    result.message =
        "Both --interval-sec and --output must be provided together.";
    return result;
  }

  result.ok = true;
  return result;
}

const char* UsageMessage() {
  return "Usage: cpu_monitor [--interval-sec N --output FILE]\n"
         "  --interval-sec N  Append CPU load samples every N seconds.\n"
         "  --output FILE     File used for periodic sample logging.\n"
         "  --help            Show this help message.\n"
         "Commands during Run state:\n"
         "  <Enter> or print  Print current per-core CPU load.\n"
         "  quit              Exit the application.\n";
}
