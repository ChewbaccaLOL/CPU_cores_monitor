#ifndef APP_H_
#define APP_H_

#include <cstddef>
#include <string>
#include <vector>

#include "args.h"
#include "cpu_reader.h"

enum class AppState {
  kInit,
  kRun,
};

class ScopedFd {
 public:
  ScopedFd() = default;
  explicit ScopedFd(int fd);
  ScopedFd(const ScopedFd&) = delete;
  ScopedFd& operator=(const ScopedFd&) = delete;
  ScopedFd(ScopedFd&& other) noexcept;
  ScopedFd& operator=(ScopedFd&& other) noexcept;
  ~ScopedFd();

  int get() const;
  bool valid() const;
  void reset(int fd = -1);

 private:
  int fd_ = -1;
};

class CpuMonitorApp {
 public:
  int Main(int argc, char* argv[]);

 private:
  bool Initialize(const AppConfig& config, std::string* error_message);
  int Run(std::string* error_message);
  bool ReadProcStat(CpuTimes* destination, std::size_t destination_size,
                    std::string* error_message);

  AppState state_ = AppState::kInit;
  AppConfig config_;
  std::size_t core_count_ = 0;
  ScopedFd proc_stat_fd_;
  std::vector<CpuTimes> previous_times_;
  std::vector<CpuTimes> current_times_;
  std::vector<double> loads_;
  std::vector<char> proc_stat_buffer_;
};

#endif  // APP_H_
