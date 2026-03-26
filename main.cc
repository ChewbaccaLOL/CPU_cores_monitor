#include "args.h"

#include <cstdio>

int main(int argc, char* argv[]) {
  const ParseResult parse_result = ParseArguments(argc, argv);
  if (!parse_result.ok) {
    std::fprintf(stderr, "%s\n%s", parse_result.message.c_str(), UsageMessage());
    return 1;
  }

  if (parse_result.show_help) {
    std::fputs(UsageMessage(), stdout);
    return 0;
  }

  return 0;
}
