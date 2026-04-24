#ifndef RUNTIME_H_
#define RUNTIME_H_

#include <cstddef>
#include <optional>
#include <sys/types.h>
#include <time.h>

class AppRuntime {
 public:
  virtual ~AppRuntime() = default;

  virtual long GetOnlineCpuCount() const = 0;
  virtual int OpenProcStat() const = 0;
  virtual int OpenOutputFile(const char* path) const = 0;
  virtual std::optional<timespec> GetMonotonicNow() const = 0;
  virtual bool GetLocalTimeNow(tm* output) const = 0;
  virtual off_t Seek(int fd, off_t offset, int whence) const = 0;
  virtual ssize_t Read(int fd, void* buffer, std::size_t count) const = 0;
  virtual ssize_t Write(int fd, const void* buffer, std::size_t count) const = 0;
  virtual bool IsTerminal(int fd) const = 0;
  virtual int WaitForStdin(const std::optional<timespec>& timeout,
                           bool stdin_open, bool* stdin_ready) const = 0;
};

class PosixAppRuntime : public AppRuntime {
 public:
  long GetOnlineCpuCount() const override;
  int OpenProcStat() const override;
  int OpenOutputFile(const char* path) const override;
  std::optional<timespec> GetMonotonicNow() const override;
  bool GetLocalTimeNow(tm* output) const override;
  off_t Seek(int fd, off_t offset, int whence) const override;
  ssize_t Read(int fd, void* buffer, std::size_t count) const override;
  ssize_t Write(int fd, const void* buffer, std::size_t count) const override;
  bool IsTerminal(int fd) const override;
  int WaitForStdin(const std::optional<timespec>& timeout, bool stdin_open,
                   bool* stdin_ready) const override;
};

#endif  // RUNTIME_H_
