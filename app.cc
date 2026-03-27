#include "app.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

namespace {

enum class CommandAction {
  kPrint,
  kQuit,
  kInvalid,
};

std::string Trim(std::string_view line) {
  std::size_t start = 0;
  while (start < line.size() &&
         std::isspace(static_cast<unsigned char>(line[start])) != 0) {
    ++start;
  }

  std::size_t end = line.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(line[end - 1])) != 0) {
    --end;
  }

  return std::string(line.substr(start, end - start));
}

CommandAction ParseCommand(std::string_view line) {
  const std::string trimmed = Trim(line);
  if (trimmed.empty() || trimmed == "print") {
    return CommandAction::kPrint;
  }
  if (trimmed == "quit") {
    return CommandAction::kQuit;
  }
  return CommandAction::kInvalid;
}

}  // namespace

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
  stdout_line_.reserve(core_count_ * 24U + 1U);
  log_line_.reserve(core_count_ * 24U + 32U);

  proc_stat_fd_.reset(open("/proc/stat", O_RDONLY | O_CLOEXEC));
  if (!proc_stat_fd_.valid()) {
    *error_message = "Unable to open /proc/stat.";
    return false;
  }

  if (config_.output_path.has_value()) {
    log_fd_.reset(open(config_.output_path->c_str(),
                       O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644));
    if (!log_fd_.valid()) {
      *error_message = "Unable to open output file.";
      return false;
    }
  }

  if (!ReadProcStat(previous_times_.data(), previous_times_.size(),
                    error_message)) {
    return false;
  }

  if (config_.interval_seconds.has_value()) {
    const std::optional<timespec> now = MonotonicNow();
    if (!now.has_value()) {
      *error_message = "Unable to read monotonic clock.";
      return false;
    }
    next_log_deadline_ = AddSeconds(*now, *config_.interval_seconds);
    if (!next_log_deadline_.has_value()) {
      *error_message = "Unable to calculate initial log deadline.";
      return false;
    }
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

  bool should_exit = false;
  while (!should_exit) {
    fd_set read_fds;
    FD_ZERO(&read_fds);

    int nfds = 0;
    if (stdin_open_) {
      FD_SET(STDIN_FILENO, &read_fds);
      nfds = STDIN_FILENO + 1;
    }

    const std::optional<timespec> timeout = NextTimeout();
    const timespec* timeout_ptr = timeout.has_value() ? &(*timeout) : nullptr;

    const int wait_result =
        pselect(nfds, stdin_open_ ? &read_fds : nullptr, nullptr, nullptr,
                timeout_ptr, nullptr);
    if (wait_result < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (error_message != nullptr) {
        *error_message = "pselect() failed.";
      }
      return 1;
    }

    if (stdin_open_ && wait_result > 0 && FD_ISSET(STDIN_FILENO, &read_fds)) {
      if (!HandleStdin(error_message, &should_exit)) {
        return 1;
      }
    }

    if (next_log_deadline_.has_value()) {
      const std::optional<timespec> now = MonotonicNow();
      if (!now.has_value()) {
        if (error_message != nullptr) {
          *error_message = "Unable to read monotonic clock.";
        }
        return 1;
      }

      while (TimeReached(*now, *next_log_deadline_)) {
        if (!EmitLogSample(error_message)) {
          return 1;
        }
        next_log_deadline_ =
            AddSeconds(*next_log_deadline_, *config_.interval_seconds);
        if (!next_log_deadline_.has_value()) {
          if (error_message != nullptr) {
            *error_message = "Unable to calculate next log deadline.";
          }
          return 1;
        }
      }
    }

    if (!stdin_open_ && !next_log_deadline_.has_value()) {
      break;
    }
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

bool CpuMonitorApp::SampleLoads(std::string* error_message) {
  if (!ReadProcStat(current_times_.data(), current_times_.size(), error_message)) {
    return false;
  }

  for (std::size_t index = 0; index < core_count_; ++index) {
    loads_[index] = ComputeLoadPercent(previous_times_[index], current_times_[index]);
  }
  previous_times_.swap(current_times_);
  return true;
}

bool CpuMonitorApp::EmitStdoutSample(std::string* error_message) {
  if (!SampleLoads(error_message)) {
    return false;
  }

  BuildOutputLine(&stdout_line_, false);
  return WriteAll(STDOUT_FILENO, stdout_line_.data(), stdout_line_.size(),
                  error_message);
}

bool CpuMonitorApp::EmitLogSample(std::string* error_message) {
  if (!log_fd_.valid()) {
    if (error_message != nullptr) {
      *error_message = "Log output requested without an open file.";
    }
    return false;
  }

  if (!SampleLoads(error_message)) {
    return false;
  }

  BuildOutputLine(&log_line_, true);
  return WriteAll(log_fd_.get(), log_line_.data(), log_line_.size(),
                  error_message);
}

bool CpuMonitorApp::WriteAll(int fd, const char* data, std::size_t size,
                             std::string* error_message) const {
  std::size_t written = 0;
  while (written < size) {
    const ssize_t rc = write(fd, data + written, size - written);
    if (rc < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (error_message != nullptr) {
        *error_message = "write() failed.";
      }
      return false;
    }
    written += static_cast<std::size_t>(rc);
  }
  return true;
}

void CpuMonitorApp::BuildOutputLine(std::string* output,
                                    bool include_timestamp) const {
  output->clear();

  if (include_timestamp) {
    const time_t now = time(nullptr);
    struct tm now_tm {};
    localtime_r(&now, &now_tm);

    char timestamp[32];
    const int timestamp_size = std::snprintf(
        timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d ",
        now_tm.tm_year + 1900, now_tm.tm_mon + 1, now_tm.tm_mday,
        now_tm.tm_hour, now_tm.tm_min, now_tm.tm_sec);
    if (timestamp_size > 0) {
      output->append(timestamp, static_cast<std::size_t>(timestamp_size));
    }
  }

  for (std::size_t index = 0; index < core_count_; ++index) {
    char entry[32];
    const int entry_size =
        std::snprintf(entry, sizeof(entry), "core%zu=%.2f%%%s", index, loads_[index],
                      (index + 1U == core_count_) ? "" : " ");
    if (entry_size > 0) {
      output->append(entry, static_cast<std::size_t>(entry_size));
    }
  }

  output->push_back('\n');
}

bool CpuMonitorApp::HandleStdin(std::string* error_message, bool* should_exit) {
  while (true) {
    if (stdin_buffer_size_ == stdin_buffer_.size()) {
      stdin_buffer_size_ = 0;
      *error_message = "Input command exceeded 255 bytes.";
      return false;
    }

    const ssize_t bytes_read = read(STDIN_FILENO,
                                    stdin_buffer_.data() + stdin_buffer_size_,
                                    stdin_buffer_.size() - stdin_buffer_size_);
    if (bytes_read < 0) {
      if (errno == EINTR) {
        continue;
      }
      *error_message = "Unable to read stdin.";
      return false;
    }
    if (bytes_read == 0) {
      stdin_open_ = false;
      return true;
    }

    stdin_buffer_size_ += static_cast<std::size_t>(bytes_read);
    break;
  }

  std::size_t line_start = 0;
  for (std::size_t index = 0; index < stdin_buffer_size_; ++index) {
    if (stdin_buffer_[index] != '\n') {
      continue;
    }

    std::size_t line_length = index - line_start;
    if (line_length > 0 && stdin_buffer_[line_start + line_length - 1] == '\r') {
      --line_length;
    }

    const std::string_view line(stdin_buffer_.data() + line_start, line_length);
    const CommandAction action = ParseCommand(line);
    if (action == CommandAction::kPrint) {
      if (!EmitStdoutSample(error_message)) {
        return false;
      }
    } else if (action == CommandAction::kQuit) {
      *should_exit = true;
    } else {
      static constexpr char kInvalidCommand[] =
          "Unknown command. Use Enter, print, or quit.\n";
      if (!WriteAll(STDERR_FILENO, kInvalidCommand, sizeof(kInvalidCommand) - 1,
                    error_message)) {
        return false;
      }
    }

    line_start = index + 1;
  }

  if (line_start > 0) {
    const std::size_t remaining = stdin_buffer_size_ - line_start;
    std::memmove(stdin_buffer_.data(), stdin_buffer_.data() + line_start, remaining);
    stdin_buffer_size_ = remaining;
  }

  return true;
}

std::optional<timespec> CpuMonitorApp::NextTimeout() const {
  if (!next_log_deadline_.has_value()) {
    return std::nullopt;
  }

  const std::optional<timespec> now = MonotonicNow();
  if (!now.has_value()) {
    return std::nullopt;
  }

  if (TimeReached(*now, *next_log_deadline_)) {
    return timespec{0, 0};
  }

  timespec delta {};
  delta.tv_sec = next_log_deadline_->tv_sec - now->tv_sec;
  delta.tv_nsec = next_log_deadline_->tv_nsec - now->tv_nsec;
  if (delta.tv_nsec < 0) {
    delta.tv_nsec += 1000000000L;
    --delta.tv_sec;
  }
  if (delta.tv_sec < 0) {
    delta.tv_sec = 0;
    delta.tv_nsec = 0;
  }
  return delta;
}

std::optional<timespec> CpuMonitorApp::MonotonicNow() {
  timespec now {};
  if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
    return std::nullopt;
  }
  return now;
}

std::optional<timespec> CpuMonitorApp::AddSeconds(const timespec& base,
                                                  unsigned int seconds) {
  timespec result = base;
  result.tv_sec += static_cast<time_t>(seconds);
  return result;
}

bool CpuMonitorApp::TimeReached(const timespec& now, const timespec& deadline) {
  if (now.tv_sec > deadline.tv_sec) {
    return true;
  }
  if (now.tv_sec < deadline.tv_sec) {
    return false;
  }
  return now.tv_nsec >= deadline.tv_nsec;
}
