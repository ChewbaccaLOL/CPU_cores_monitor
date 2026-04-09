#ifndef APP_H_
#define APP_H_

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <time.h>
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

class AppRuntime {
 public:
  virtual ~AppRuntime() = default;

  virtual long GetOnlineCpuCount() const = 0;
  virtual int OpenProcStat() const = 0;
  virtual int OpenOutputFile(const char* path) const = 0;
  virtual std::optional<timespec> GetMonotonicNow() const = 0;
};

class CpuMonitorApp {
 public:
  CpuMonitorApp();
  explicit CpuMonitorApp(AppRuntime& runtime);
  virtual ~CpuMonitorApp() = default;
  int Main(int argc, char* argv[]);

 protected:
  virtual bool Initialize(const AppConfig& config, std::string* error_message);
  virtual int Run(std::string* error_message);

 private:
  int WaitForEvents(std::string* error_message, bool* stdin_ready) const;
  bool HandleScheduledLogging(std::string* error_message);
  bool ReadProcStat(CpuTimes* destination, std::size_t destination_size,
                    std::string* error_message);
  bool SampleLoadsUsingBaseline(std::vector<CpuTimes>* previous_times,
                                std::string* error_message);
  bool EmitStdoutSample(std::string* error_message);
  bool EmitLogSample(std::string* error_message);
  bool WriteAll(int fd, const char* data, std::size_t size,
                std::string* error_message) const;
  void BuildOutputLine(std::string* output, bool include_timestamp) const;
  bool HandleStdin(std::string* error_message, bool* should_exit);
  std::optional<timespec> NextTimeout() const;
  static std::optional<timespec> MonotonicNow();
  static std::optional<timespec> AddSeconds(const timespec& base,
                                            unsigned int seconds);
  static bool TimeReached(const timespec& now, const timespec& deadline);

  AppState state_ = AppState::kInit;
  AppConfig config_;
  std::size_t core_count_ = 0;
  ScopedFd proc_stat_fd_;
  ScopedFd log_fd_;
  std::vector<CpuTimes> previous_stdout_times_;
  std::vector<CpuTimes> previous_log_times_;
  std::vector<CpuTimes> current_times_;
  std::vector<double> loads_;
  std::vector<char> proc_stat_buffer_;
  std::string stdout_line_;
  std::string log_line_;
  std::array<char, 256> stdin_buffer_{};
  std::size_t stdin_buffer_size_ = 0;
  bool stdin_open_ = true;
  std::optional<timespec> next_log_deadline_;
  AppRuntime* runtime_ = nullptr;
};

#endif  // APP_H_
