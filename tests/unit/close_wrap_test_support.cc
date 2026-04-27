#include "tests/unit/close_wrap_test_support.h"

#include <sys/syscall.h>
#include <unistd.h>

namespace {

CloseMock*& ActiveCloseMock() {
  static CloseMock* mock = nullptr;
  return mock;
}

}  // namespace

CloseMock* SetActiveCloseMock(CloseMock* mock) {
  CloseMock* previous = ActiveCloseMock();
  ActiveCloseMock() = mock;
  return previous;
}

extern "C" int __wrap_close(int fd) {
  CloseMock* mock = ActiveCloseMock();
  if (mock == nullptr) {
    return static_cast<int>(syscall(SYS_close, fd));
  }
  return mock->Close(fd);
}
