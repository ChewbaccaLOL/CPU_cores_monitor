#include "app.h"

#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

ScopedFd::ScopedFd(int fd) : fd_(fd) {}

ScopedFd::ScopedFd(ScopedFd&& other) noexcept : fd_(other.fd_) {
  other.fd_ = -1;
}

ScopedFd& ScopedFd::operator=(ScopedFd&& other) noexcept {
  if (this != &other) {
    reset();
    fd_ = other.fd_;
    other.fd_ = -1;
  }
  return *this;
}

ScopedFd::~ScopedFd() { reset(); }

int ScopedFd::get() const { return fd_; }

bool ScopedFd::valid() const { return fd_ >= 0; }

void ScopedFd::reset(int fd) {
  if (fd_ >= 0) {
    close(fd_);
  }
  fd_ = fd;
}

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
  if (error_message == nullptr) {
    return false;
  }

  config_ = config;
  state_ = AppState::kInit;

  const long detected_cores = sysconf(_SC_NPROCESSORS_ONLN);
  if (detected_cores <= 0) {
    *error_message = "Unable to detect online CPU cores.";
    return false;
  }
  core_count_ = static_cast<std::size_t>(detected_cores);

  previous_times_.assign(core_count_, CpuTimes{});
  current_times_.assign(core_count_, CpuTimes{});
  loads_.assign(core_count_, 0.0);
  proc_stat_buffer_.assign((core_count_ + 2U) * 256U, '\0');

  proc_stat_fd_.reset(open("/proc/stat", O_RDONLY | O_CLOEXEC));
  if (!proc_stat_fd_.valid()) {
    *error_message = "Unable to open /proc/stat.";
    return false;
  }

  if (!ReadProcStat(previous_times_.data(), previous_times_.size(),
                    error_message)) {
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

bool CpuMonitorApp::ReadProcStat(CpuTimes* destination,
                                 std::size_t destination_size,
                                 std::string* error_message) {
  if (lseek(proc_stat_fd_.get(), 0, SEEK_SET) < 0) {
    *error_message = "Unable to seek /proc/stat.";
    return false;
  }

  std::size_t total_read = 0;
  while (total_read < proc_stat_buffer_.size()) {
    const ssize_t bytes_read =
        read(proc_stat_fd_.get(), proc_stat_buffer_.data() + total_read,
             proc_stat_buffer_.size() - total_read);
    if (bytes_read < 0) {
      if (errno == EINTR) {
        continue;
      }
      *error_message = "Unable to read /proc/stat.";
      return false;
    }
    if (bytes_read == 0) {
      break;
    }
    total_read += static_cast<std::size_t>(bytes_read);
  }

  if (total_read == proc_stat_buffer_.size()) {
    *error_message = "/proc/stat buffer was too small.";
    return false;
  }

  std::size_t parsed_core_count = 0;
  if (!ParseProcStatBuffer(proc_stat_buffer_.data(), total_read, destination,
                           destination_size, &parsed_core_count)) {
    *error_message = "Unable to parse /proc/stat.";
    return false;
  }

  if (parsed_core_count != destination_size) {
    *error_message = "Parsed core count does not match detected core count.";
    return false;
  }

  return true;
}
