#include "runtime.h"

#include <cerrno>
#include <fcntl.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

long PosixAppRuntime::GetOnlineCpuCount() const {
  return sysconf(_SC_NPROCESSORS_ONLN);
}

int PosixAppRuntime::OpenProcStat() const {
  return open("/proc/stat", O_RDONLY | O_CLOEXEC);
}

int PosixAppRuntime::OpenOutputFile(const char* path) const {
  return open(path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
}

std::optional<timespec> PosixAppRuntime::GetMonotonicNow() const {
  timespec now {};
  if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
    return std::nullopt;
  }
  return now;
}

bool PosixAppRuntime::GetLocalTimeNow(tm* output) const {
  if (output == nullptr) {
    return false;
  }

  const time_t now = time(nullptr);
  return localtime_r(&now, output) != nullptr;
}

off_t PosixAppRuntime::Seek(int fd, off_t offset, int whence) const {
  return lseek(fd, offset, whence);
}

ssize_t PosixAppRuntime::Read(int fd, void* buffer, std::size_t count) const {
  return read(fd, buffer, count);
}

ssize_t PosixAppRuntime::Write(int fd, const void* buffer,
                               std::size_t count) const {
  return write(fd, buffer, count);
}

bool PosixAppRuntime::IsTerminal(int fd) const { return isatty(fd) != 0; }

int PosixAppRuntime::WaitForStdin(const std::optional<timespec>& timeout,
                                  bool stdin_open, bool* stdin_ready) const {
  if (stdin_ready == nullptr) {
    return -1;
  }

  *stdin_ready = false;

  fd_set read_fds;
  FD_ZERO(&read_fds);

  int nfds = 0;
  if (stdin_open) {
    FD_SET(STDIN_FILENO, &read_fds);
    nfds = STDIN_FILENO + 1;
  }

  const timespec* timeout_ptr = timeout.has_value() ? &(*timeout) : nullptr;
  const int wait_result =
      pselect(nfds, stdin_open ? &read_fds : nullptr, nullptr, nullptr,
              timeout_ptr, nullptr);
  if (wait_result < 0) {
    if (errno == EINTR) {
      return 0;
    }
    return -1;
  }

  *stdin_ready = stdin_open && wait_result > 0 && FD_ISSET(STDIN_FILENO, &read_fds);
  return wait_result;
}
