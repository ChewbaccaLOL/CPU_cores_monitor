#include "app.h"

#include <cerrno>
#include <cstring>
#include <cstdio>
#include <fcntl.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <unistd.h>

namespace {

using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::StrEq;

constexpr char kBaselineProcStat[] =
    "cpu  100 0 50 400 0 0 0 0\n"
    "cpu0 10 1 2 30 4 5 6 7\n"
    "cpu1 20 3 4 40 5 6 7 8\n";

constexpr char kUpdatedProcStat[] =
    "cpu  200 0 100 800 0 0 0 0\n"
    "cpu0 20 1 12 50 4 5 6 7\n"
    "cpu1 30 3 14 60 5 6 7 8\n";

class TempFile {
 public:
  TempFile() {
    char path_template[] = "/tmp/cpu_monitor_test_XXXXXX";
    const int fd = mkstemp(path_template);
    if (fd < 0) {
      return;
    }

    path_ = path_template;
    close(fd);
  }

  ~TempFile() {
    if (!path_.empty()) {
      std::remove(path_.c_str());
    }
  }

  bool valid() const { return !path_.empty(); }

  bool WriteContents(const std::string& contents) const {
    const int fd = open(path_.c_str(), O_WRONLY | O_TRUNC);
    if (fd < 0) {
      return false;
    }

    std::size_t total_written = 0;
    while (total_written < contents.size()) {
      const ssize_t bytes_written =
          write(fd, contents.data() + total_written, contents.size() - total_written);
      if (bytes_written < 0) {
        if (errno == EINTR) {
          continue;
        }
        close(fd);
        return false;
      }
      total_written += static_cast<std::size_t>(bytes_written);
    }

    close(fd);
    return true;
  }

  int OpenReadOnly() const { return open(path_.c_str(), O_RDONLY | O_CLOEXEC); }

  const char* path() const { return path_.c_str(); }

 private:
  std::string path_;
};

class MockCpuMonitorApp : public CpuMonitorApp {
 public:
  explicit MockCpuMonitorApp(AppRuntime& runtime) : CpuMonitorApp(runtime) {}

  MOCK_METHOD(bool, Initialize, (const AppConfig& config, std::string* error),
              (override));
  MOCK_METHOD(int, Run, (std::string* error), (override));
};

class MockAppRuntime : public AppRuntime {
 public:
  MOCK_METHOD(long, GetOnlineCpuCount, (), (const, override));
  MOCK_METHOD(int, OpenProcStat, (), (const, override));
  MOCK_METHOD(int, OpenOutputFile, (const char* path), (const, override));
  MOCK_METHOD(int, Close, (int fd), (const, override));
  MOCK_METHOD(std::optional<timespec>, GetMonotonicNow, (), (const, override));
  MOCK_METHOD(bool, GetLocalTimeNow, (tm * output), (const, override));
  MOCK_METHOD(off_t, Seek, (int fd, off_t offset, int whence), (const, override));
  MOCK_METHOD(ssize_t, Read, (int fd, void* buffer, std::size_t count),
              (const, override));
  MOCK_METHOD(ssize_t, Write, (int fd, const void* buffer, std::size_t count),
              (const, override));
  MOCK_METHOD(bool, IsTerminal, (int fd), (const, override));
  MOCK_METHOD(int, WaitForStdin,
              (const std::optional<timespec>& timeout, bool stdin_open,
               bool* stdin_ready),
              (const, override));
};

using NiceMockAppRuntime = ::testing::NiceMock<MockAppRuntime>;

void ExpectProcStatFileAccess(MockAppRuntime& runtime, const TempFile& proc_stat) {
  EXPECT_CALL(runtime, OpenProcStat())
      .WillOnce([&proc_stat]() { return proc_stat.OpenReadOnly(); });
  EXPECT_CALL(runtime, Seek(_, 0, SEEK_SET))
      .WillOnce([](int fd, off_t offset, int whence) {
        return lseek(fd, offset, whence);
      });
  EXPECT_CALL(runtime, Read(_, _, _))
      .WillRepeatedly([](int fd, void* buffer, std::size_t count) {
        return read(fd, buffer, count);
      });
}

class TestableCpuMonitorApp : public CpuMonitorApp {
 public:
  explicit TestableCpuMonitorApp(AppRuntime& runtime) : CpuMonitorApp(runtime) {}

  using CpuMonitorApp::Initialize;
  using CpuMonitorApp::Run;
};

class CpuMonitorAppInitializedRunTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(proc_stat_.valid());
    ASSERT_TRUE(proc_stat_.WriteContents(kBaselineProcStat));
    proc_fd_ = proc_stat_.OpenReadOnly();
    ASSERT_GE(proc_fd_, 0);
  }

  void InitializeApp() {
    EXPECT_CALL(runtime_, GetOnlineCpuCount()).WillOnce(Return(2));
    EXPECT_CALL(runtime_, OpenProcStat()).WillOnce(Return(proc_fd_));
    EXPECT_CALL(runtime_, Seek(proc_fd_, 0, SEEK_SET)).WillOnce(Return(0));
    EXPECT_CALL(runtime_, Read(proc_fd_, _, _))
        .WillRepeatedly([](int fd, void* buffer, std::size_t count) {
          return read(fd, buffer, count);
        });
    EXPECT_CALL(runtime_, OpenOutputFile(_)).Times(0);
    EXPECT_CALL(runtime_, GetMonotonicNow()).Times(0);

    ASSERT_TRUE(app_.Initialize(AppConfig{}, &error_message_));
    ASSERT_TRUE(error_message_.empty());

    testing::Mock::VerifyAndClearExpectations(&runtime_);
  }

  void ExpectNonInteractiveStartup() {
    EXPECT_CALL(runtime_, IsTerminal(STDIN_FILENO)).WillOnce(Return(false));
    EXPECT_CALL(runtime_, IsTerminal(STDOUT_FILENO)).Times(0);
  }

  void ExpectStdinReady() {
    EXPECT_CALL(runtime_, WaitForStdin(_, true, _))
        .WillOnce([](const std::optional<timespec>&, bool, bool* stdin_ready) {
          *stdin_ready = true;
          return 1;
        });
  }

  void ExpectStdinReadyTwice() {
    EXPECT_CALL(runtime_, WaitForStdin(_, true, _))
        .WillOnce([](const std::optional<timespec>&, bool, bool* stdin_ready) {
          *stdin_ready = true;
          return 1;
        })
        .WillOnce([](const std::optional<timespec>&, bool, bool* stdin_ready) {
          *stdin_ready = true;
          return 1;
        });
  }

  TempFile proc_stat_;
  int proc_fd_ = -1;
  NiceMockAppRuntime runtime_;
  TestableCpuMonitorApp app_{runtime_};
  std::string error_message_;
};

class CpuMonitorAppPrintRunTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(proc_stat_.valid());
    ASSERT_TRUE(WriteBaselineProcStat());
    proc_fd_ = proc_stat_.OpenReadOnly();
    ASSERT_GE(proc_fd_, 0);
  }

  bool WriteBaselineProcStat() {
    return proc_stat_.WriteContents(kBaselineProcStat);
  }

  bool WriteUpdatedProcStat() {
    return proc_stat_.WriteContents(kUpdatedProcStat);
  }

  void InitializeApp() {
    EXPECT_CALL(runtime_, GetOnlineCpuCount()).WillOnce(Return(2));
    EXPECT_CALL(runtime_, OpenProcStat()).WillOnce(Return(proc_fd_));
    EXPECT_CALL(runtime_, Seek(proc_fd_, 0, SEEK_SET))
        .WillOnce([](int fd, off_t offset, int whence) {
          return lseek(fd, offset, whence);
        });
    EXPECT_CALL(runtime_, Read(proc_fd_, _, _))
        .WillRepeatedly([](int fd, void* buffer, std::size_t count) {
          return read(fd, buffer, count);
        });
    EXPECT_CALL(runtime_, OpenOutputFile(_)).Times(0);
    EXPECT_CALL(runtime_, GetMonotonicNow()).Times(0);

    ASSERT_TRUE(app_.Initialize(AppConfig{}, &error_message_));
    ASSERT_TRUE(error_message_.empty());

    testing::Mock::VerifyAndClearExpectations(&runtime_);
  }

  void ExpectNonInteractiveStartup() {
    EXPECT_CALL(runtime_, IsTerminal(STDIN_FILENO)).WillOnce(Return(false));
    EXPECT_CALL(runtime_, IsTerminal(STDOUT_FILENO)).Times(0);
  }

  void ExpectStdinReady() {
    EXPECT_CALL(runtime_, WaitForStdin(_, true, _))
        .WillOnce([](const std::optional<timespec>&, bool, bool* stdin_ready) {
          *stdin_ready = true;
          return 1;
        });
  }

  void ExpectPrintCommand() {
    EXPECT_CALL(runtime_, Read(STDIN_FILENO, _, _))
        .WillOnce([](int, void* buffer, std::size_t) {
          static constexpr char kPrintCommand[] = "print\n";
          std::memcpy(buffer, kPrintCommand, sizeof(kPrintCommand) - 1);
          return static_cast<ssize_t>(sizeof(kPrintCommand) - 1);
        });
  }

  void ExpectPrintCommandThenEof() {
    EXPECT_CALL(runtime_, Read(STDIN_FILENO, _, _))
        .WillOnce([](int, void* buffer, std::size_t) {
          static constexpr char kPrintCommand[] = "print\n";
          std::memcpy(buffer, kPrintCommand, sizeof(kPrintCommand) - 1);
          return static_cast<ssize_t>(sizeof(kPrintCommand) - 1);
        })
        .WillOnce(Return(0));
  }

  void ExpectProcStatSeek() {
    EXPECT_CALL(runtime_, Seek(proc_fd_, 0, SEEK_SET))
        .WillOnce([](int fd, off_t offset, int whence) {
          return lseek(fd, offset, whence);
        });
  }

  void ExpectProcStatRead() {
    EXPECT_CALL(runtime_, Read(proc_fd_, _, _))
        .WillRepeatedly([](int fd, void* buffer, std::size_t count) {
          return read(fd, buffer, count);
        });
  }

  TempFile proc_stat_;
  int proc_fd_ = -1;
  NiceMockAppRuntime runtime_;
  TestableCpuMonitorApp app_{runtime_};
  std::string error_message_;
};

class CpuMonitorAppScheduledLogRunTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(proc_stat_.valid());
    ASSERT_TRUE(output_file_.valid());
    ASSERT_TRUE(proc_stat_.WriteContents(kBaselineProcStat));
    proc_fd_ = proc_stat_.OpenReadOnly();
    ASSERT_GE(proc_fd_, 0);
    config_.interval_seconds = 5;
    config_.output_path = std::string(output_file_.path());
  }

  bool WriteUpdatedProcStat() {
    return proc_stat_.WriteContents(kUpdatedProcStat);
  }

  void InitializeApp() {
    EXPECT_CALL(runtime_, GetOnlineCpuCount()).WillOnce(Return(2));
    EXPECT_CALL(runtime_, OpenProcStat()).WillOnce(Return(proc_fd_));
    EXPECT_CALL(runtime_, Seek(proc_fd_, 0, SEEK_SET))
        .WillOnce([](int fd, off_t offset, int whence) {
          return lseek(fd, offset, whence);
        });
    EXPECT_CALL(runtime_, Read(proc_fd_, _, _))
        .WillRepeatedly([](int fd, void* buffer, std::size_t count) {
          return read(fd, buffer, count);
        });
    EXPECT_CALL(runtime_, OpenOutputFile(StrEq(output_file_.path())))
        .WillOnce([this](const char*) {
          log_fd_ =
              open(output_file_.path(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC,
                   0644);
          return log_fd_;
        });
    EXPECT_CALL(runtime_, GetMonotonicNow())
        .WillOnce(Return(std::optional<timespec>{timespec{10, 20}}));
    EXPECT_CALL(runtime_, GetLocalTimeNow(_)).WillOnce(Return(true));

    ASSERT_TRUE(app_.Initialize(config_, &error_message_));
    ASSERT_TRUE(error_message_.empty());
    ASSERT_GE(log_fd_, 0);

    testing::Mock::VerifyAndClearExpectations(&runtime_);
  }

  void ExpectNonInteractiveStartup() {
    EXPECT_CALL(runtime_, IsTerminal(STDIN_FILENO)).WillOnce(Return(false));
    EXPECT_CALL(runtime_, IsTerminal(STDOUT_FILENO)).Times(0);
  }

  void ExpectProcStatSeek() {
    EXPECT_CALL(runtime_, Seek(proc_fd_, 0, SEEK_SET))
        .WillOnce([](int fd, off_t offset, int whence) {
          return lseek(fd, offset, whence);
        });
  }

  void ExpectProcStatRead() {
    EXPECT_CALL(runtime_, Read(proc_fd_, _, _))
        .WillRepeatedly([](int fd, void* buffer, std::size_t count) {
          return read(fd, buffer, count);
        });
  }

  TempFile proc_stat_;
  TempFile output_file_;
  int proc_fd_ = -1;
  int log_fd_ = -1;
  NiceMockAppRuntime runtime_;
  TestableCpuMonitorApp app_{runtime_};
  std::string error_message_;
  AppConfig config_;
};

TEST(CpuMonitorAppMainTest, HelpFlagPrintsUsageWithoutInitializing) {
  NiceMockAppRuntime runtime;
  MockCpuMonitorApp app(runtime);
  char program[] = "cpu_monitor";
  char help[] = "--help";
  char* argv[] = {program, help};

  EXPECT_CALL(app, Initialize(_, _)).Times(0);
  EXPECT_CALL(app, Run(_)).Times(0);

  testing::internal::CaptureStdout();
  const int exit_code = app.Main(2, argv);
  const std::string stdout_output = testing::internal::GetCapturedStdout();

  EXPECT_EQ(exit_code, 0);
  EXPECT_EQ(stdout_output, std::string(UsageMessage()));
}

TEST(CpuMonitorAppMainTest, ParseFailurePrintsErrorAndUsageToStderr) {
  NiceMockAppRuntime runtime;
  MockCpuMonitorApp app(runtime);
  char program[] = "cpu_monitor";
  char invalid_flag[] = "--ummm";
  char* argv[] = {program, invalid_flag};

  EXPECT_CALL(app, Initialize(_, _)).Times(0);
  EXPECT_CALL(app, Run(_)).Times(0);

  testing::internal::CaptureStderr();
  const int exit_code = app.Main(2, argv);
  const std::string stderr_output = testing::internal::GetCapturedStderr();

  EXPECT_EQ(exit_code, 1);
  EXPECT_THAT(stderr_output, HasSubstr("Unknown argument: --ummm"));
  EXPECT_THAT(stderr_output, HasSubstr(UsageMessage()));
}

TEST(CpuMonitorAppMainTest, PartialLoggingFlagsPrintErrorAndUsageToStderr) {
  NiceMockAppRuntime runtime;
  MockCpuMonitorApp app(runtime);
  char program[] = "cpu_monitor";
  char interval_flag[] = "--interval-sec";
  char interval_value[] = "5";
  char* argv[] = {program, interval_flag, interval_value};

  EXPECT_CALL(app, Initialize(_, _)).Times(0);
  EXPECT_CALL(app, Run(_)).Times(0);

  testing::internal::CaptureStderr();
  const int exit_code = app.Main(3, argv);
  const std::string stderr_output = testing::internal::GetCapturedStderr();

  EXPECT_EQ(exit_code, 1);
  EXPECT_THAT(stderr_output,
              HasSubstr("Both --interval-sec and --output must be provided "
                        "together."));
  EXPECT_THAT(stderr_output, HasSubstr(UsageMessage()));
}

TEST(CpuMonitorAppMainTest, ValidArgsInitializeAndRun) {
  NiceMockAppRuntime runtime;
  MockCpuMonitorApp app(runtime);
  char program[] = "cpu_monitor";
  char interval_flag[] = "--interval-sec";
  char interval_value[] = "5";
  char output_flag[] = "--output";
  char output_value[] = "cpu.log";
  char* argv[] = {
      program,
      interval_flag,
      interval_value,
      output_flag,
      output_value,
  };

  EXPECT_CALL(app, Initialize(_, _))
      .WillOnce([](const AppConfig& config, std::string*) {
        EXPECT_TRUE(config.interval_seconds.has_value());
        EXPECT_TRUE(config.output_path.has_value());
        EXPECT_EQ(*config.interval_seconds, 5U);
        EXPECT_EQ(*config.output_path, "cpu.log");
        return true;
      });
  EXPECT_CALL(app, Run(_)).WillOnce([](std::string*) { return 0; });

  EXPECT_EQ(app.Main(5, argv), 0);
}

TEST(CpuMonitorAppMainTest, InitializeFailureReturnsError) {
  NiceMockAppRuntime runtime;
  MockCpuMonitorApp app(runtime);
  char program[] = "cpu_monitor";
  char* argv[] = {program};

  EXPECT_CALL(app, Initialize(_, _))
      .WillOnce([](const AppConfig&, std::string* error) {
        *error = "some_error";
        return false;
      });
  EXPECT_CALL(app, Run(_)).Times(0);

  testing::internal::CaptureStderr();
  const int exit_code = app.Main(1, argv);
  const std::string stderr_output = testing::internal::GetCapturedStderr();

  EXPECT_EQ(exit_code, 1);
  EXPECT_THAT(stderr_output, HasSubstr("Init failed: some_error"));
}

TEST(CpuMonitorAppMainTest, RunFailurePropagatesExitCodeAndMessage) {
  NiceMockAppRuntime runtime;
  MockCpuMonitorApp app(runtime);
  char program[] = "cpu_monitor";
  char* argv[] = {program};

  EXPECT_CALL(app, Initialize(_, _))
      .WillOnce([](const AppConfig&, std::string*) { return true; });
  EXPECT_CALL(app, Run(_)).WillOnce([](std::string* error) {
    *error = "run some_error";
    return 1;
  });

  testing::internal::CaptureStderr();
  const int exit_code = app.Main(1, argv);
  const std::string stderr_output = testing::internal::GetCapturedStderr();

  EXPECT_EQ(exit_code, 1);
  EXPECT_THAT(stderr_output, HasSubstr("Run failed: run some_error"));
}

TEST(CpuMonitorAppInitializeTest, FailsWhenCpuCountDetectionFails) {
  NiceMockAppRuntime runtime;
  TestableCpuMonitorApp app(runtime);
  std::string error_message;

  EXPECT_CALL(runtime, GetOnlineCpuCount()).WillOnce(Return(0));
  EXPECT_CALL(runtime, OpenProcStat()).Times(0);
  EXPECT_CALL(runtime, OpenOutputFile(_)).Times(0);
  EXPECT_CALL(runtime, GetMonotonicNow()).Times(0);

  EXPECT_FALSE(app.Initialize(AppConfig{}, &error_message));
  EXPECT_EQ(error_message, "Unable to detect online CPU cores.");
}

TEST(CpuMonitorAppInitializeTest, FailsWhenProcStatOpenFails) {
  NiceMockAppRuntime runtime;
  TestableCpuMonitorApp app(runtime);
  std::string error_message;

  EXPECT_CALL(runtime, GetOnlineCpuCount()).WillOnce(Return(2));
  EXPECT_CALL(runtime, OpenProcStat()).WillOnce(Return(-1));
  EXPECT_CALL(runtime, OpenOutputFile(_)).Times(0);
  EXPECT_CALL(runtime, GetMonotonicNow()).Times(0);

  EXPECT_FALSE(app.Initialize(AppConfig{}, &error_message));
  EXPECT_EQ(error_message, "Unable to open /proc/stat.");
}

TEST(CpuMonitorAppInitializeTest, FailsWhenOutputFileOpenFails) {
  NiceMockAppRuntime runtime;
  TestableCpuMonitorApp app(runtime);
  std::string error_message;
  AppConfig config;
  config.output_path = "cpu.log";

  EXPECT_CALL(runtime, GetOnlineCpuCount()).WillOnce(Return(2));
  EXPECT_CALL(runtime, OpenProcStat()).WillOnce(Return(10));
  EXPECT_CALL(runtime, OpenOutputFile(_)).WillOnce(Return(-1));
  EXPECT_CALL(runtime, GetMonotonicNow()).Times(0);

  EXPECT_FALSE(app.Initialize(config, &error_message));
  EXPECT_EQ(error_message, "Unable to open output file.");
}

TEST(CpuMonitorAppInitializeTest, SucceedsWithValidProcStatData) {
  TempFile proc_stat;
  ASSERT_TRUE(proc_stat.valid());
  ASSERT_TRUE(proc_stat.WriteContents(
      "cpu  100 0 50 400 0 0 0 0\n"
      "cpu0 10 1 2 30 4 5 6 7\n"
      "cpu1 20 3 4 40 5 6 7 8\n"));

  NiceMockAppRuntime runtime;
  TestableCpuMonitorApp app(runtime);
  std::string error_message;

  EXPECT_CALL(runtime, GetOnlineCpuCount()).WillOnce(Return(2));
  ExpectProcStatFileAccess(runtime, proc_stat);
  EXPECT_CALL(runtime, OpenOutputFile(_)).Times(0);
  EXPECT_CALL(runtime, GetMonotonicNow()).Times(0);

  EXPECT_TRUE(app.Initialize(AppConfig{}, &error_message));
  EXPECT_TRUE(error_message.empty());
}

TEST(CpuMonitorAppInitializeTest, FailsWhenMonotonicClockReadFails) {
  TempFile proc_stat;
  ASSERT_TRUE(proc_stat.valid());
  ASSERT_TRUE(proc_stat.WriteContents(
      "cpu  100 0 50 400 0 0 0 0\n"
      "cpu0 10 1 2 30 4 5 6 7\n"
      "cpu1 20 3 4 40 5 6 7 8\n"));

  NiceMockAppRuntime runtime;
  TestableCpuMonitorApp app(runtime);
  std::string error_message;
  AppConfig config;
  config.interval_seconds = 5;

  EXPECT_CALL(runtime, GetOnlineCpuCount()).WillOnce(Return(2));
  ExpectProcStatFileAccess(runtime, proc_stat);
  EXPECT_CALL(runtime, OpenOutputFile(_)).Times(0);
  EXPECT_CALL(runtime, GetMonotonicNow())
      .WillOnce(Return(std::optional<timespec>{}));

  EXPECT_FALSE(app.Initialize(config, &error_message));
  EXPECT_EQ(error_message, "Unable to read monotonic clock.");
}

TEST(CpuMonitorAppInitializeTest, FailsWhenProcStatCannotBeParsed) {
  TempFile proc_stat;
  ASSERT_TRUE(proc_stat.valid());
  ASSERT_TRUE(proc_stat.WriteContents("not a proc stat buffer\n"));

  NiceMockAppRuntime runtime;
  TestableCpuMonitorApp app(runtime);
  std::string error_message;

  EXPECT_CALL(runtime, GetOnlineCpuCount()).WillOnce(Return(2));
  ExpectProcStatFileAccess(runtime, proc_stat);
  EXPECT_CALL(runtime, OpenOutputFile(_)).Times(0);
  EXPECT_CALL(runtime, GetMonotonicNow()).Times(0);

  EXPECT_FALSE(app.Initialize(AppConfig{}, &error_message));
  EXPECT_EQ(error_message, "Unable to parse /proc/stat.");
}

TEST(CpuMonitorAppInitializeTest, FailsWhenParsedCoreCountDoesNotMatch) {
  TempFile proc_stat;
  ASSERT_TRUE(proc_stat.valid());
  ASSERT_TRUE(proc_stat.WriteContents(
      "cpu  100 0 50 400 0 0 0 0\n"
      "cpu0 10 1 2 30 4 5 6 7\n"));

  NiceMockAppRuntime runtime;
  TestableCpuMonitorApp app(runtime);
  std::string error_message;

  EXPECT_CALL(runtime, GetOnlineCpuCount()).WillOnce(Return(2));
  ExpectProcStatFileAccess(runtime, proc_stat);
  EXPECT_CALL(runtime, OpenOutputFile(_)).Times(0);
  EXPECT_CALL(runtime, GetMonotonicNow()).Times(0);

  EXPECT_FALSE(app.Initialize(AppConfig{}, &error_message));
  EXPECT_EQ(error_message, "Parsed core count does not match detected core count.");
}

TEST(CpuMonitorAppInitializeTest, SucceedsWithPeriodicLoggingConfigured) {
  TempFile proc_stat;
  ASSERT_TRUE(proc_stat.valid());
  ASSERT_TRUE(proc_stat.WriteContents(
      "cpu  100 0 50 400 0 0 0 0\n"
      "cpu0 10 1 2 30 4 5 6 7\n"
      "cpu1 20 3 4 40 5 6 7 8\n"));

  TempFile output_file;
  ASSERT_TRUE(output_file.valid());

  NiceMockAppRuntime runtime;
  TestableCpuMonitorApp app(runtime);
  std::string error_message;
  AppConfig config;
  config.interval_seconds = 5;
  config.output_path = std::string(output_file.path());

  EXPECT_CALL(runtime, GetOnlineCpuCount()).WillOnce(Return(2));
  ExpectProcStatFileAccess(runtime, proc_stat);
  EXPECT_CALL(runtime, OpenOutputFile(StrEq(output_file.path())))
      .WillOnce([&output_file](const char*) {
        return open(output_file.path(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC,
                    0644);
      });
  EXPECT_CALL(runtime, GetMonotonicNow())
      .WillOnce(Return(std::optional<timespec>{timespec{10, 20}}));
  EXPECT_CALL(runtime, GetLocalTimeNow(_)).WillOnce(Return(true));

  EXPECT_TRUE(app.Initialize(config, &error_message));
  EXPECT_TRUE(error_message.empty());
}

TEST(CpuMonitorAppRunTest, ReturnsErrorWhenCalledBeforeInitialize) {
  NiceMockAppRuntime runtime;
  TestableCpuMonitorApp app(runtime);
  std::string error_message;

  EXPECT_EQ(app.Run(&error_message), 1);
  EXPECT_EQ(error_message, "Application entered Run before Init completed.");
}

TEST_F(CpuMonitorAppInitializedRunTest, ReturnsErrorWhenWaitingForStdinFails) {
  InitializeApp();

  ExpectNonInteractiveStartup();
  EXPECT_CALL(runtime_, WaitForStdin(_, true, _)).WillOnce(Return(-1));
  EXPECT_CALL(runtime_, Read(STDIN_FILENO, _, _)).Times(0);
  EXPECT_CALL(runtime_, Write(_, _, _)).Times(0);
  EXPECT_CALL(runtime_, GetMonotonicNow()).Times(0);

  EXPECT_EQ(app_.Run(&error_message_), 1);
  EXPECT_EQ(error_message_, "pselect() failed.");
}

TEST_F(CpuMonitorAppInitializedRunTest, ReturnsErrorWhenReadingStdinFails) {
  InitializeApp();

  ExpectNonInteractiveStartup();
  ExpectStdinReady();
  EXPECT_CALL(runtime_, Read(STDIN_FILENO, _, _))
      .WillOnce([](int, void*, std::size_t) {
        errno = EIO;
        return -1;
      });
  EXPECT_CALL(runtime_, Write(_, _, _)).Times(0);
  EXPECT_CALL(runtime_, GetMonotonicNow()).Times(0);

  EXPECT_EQ(app_.Run(&error_message_), 1);
  EXPECT_EQ(error_message_, "Unable to read stdin.");
}

TEST_F(CpuMonitorAppInitializedRunTest, ExitsWhenQuitCommandIsReadFromStdin) {
  InitializeApp();

  ExpectNonInteractiveStartup();
  ExpectStdinReady();
  EXPECT_CALL(runtime_, Read(STDIN_FILENO, _, _))
      .WillOnce([](int, void* buffer, std::size_t) {
        static constexpr char kQuitCommand[] = "quit\n";
        std::memcpy(buffer, kQuitCommand, sizeof(kQuitCommand) - 1);
        return static_cast<ssize_t>(sizeof(kQuitCommand) - 1);
      });
  EXPECT_CALL(runtime_, GetMonotonicNow()).Times(0);
  EXPECT_CALL(runtime_, Write(_, _, _)).Times(0);

  EXPECT_EQ(app_.Run(&error_message_), 0);
  EXPECT_TRUE(error_message_.empty());
}

TEST_F(CpuMonitorAppInitializedRunTest, WritesErrorForInvalidCommand) {
  InitializeApp();

  ExpectNonInteractiveStartup();
  ExpectStdinReadyTwice();
  EXPECT_CALL(runtime_, Read(STDIN_FILENO, _, _))
      .WillOnce([](int, void* buffer, std::size_t) {
        static constexpr char kInvalidCommand[] = "oops\n";
        std::memcpy(buffer, kInvalidCommand, sizeof(kInvalidCommand) - 1);
        return static_cast<ssize_t>(sizeof(kInvalidCommand) - 1);
      })
      .WillOnce(Return(0));
  EXPECT_CALL(runtime_, GetMonotonicNow()).Times(0);
  EXPECT_CALL(runtime_, Write(STDERR_FILENO, _, _))
      .WillOnce([](int, const void* buffer, std::size_t count) {
        EXPECT_EQ(std::string(static_cast<const char*>(buffer), count),
                  "Unknown command. Use Enter, print, or quit.\n");
        return static_cast<ssize_t>(count);
      });

  EXPECT_EQ(app_.Run(&error_message_), 0);
  EXPECT_TRUE(error_message_.empty());
}

TEST_F(CpuMonitorAppPrintRunTest, PrintsCpuSampleForPrintCommand) {
  InitializeApp();
  ASSERT_TRUE(WriteUpdatedProcStat());

  ExpectNonInteractiveStartup();
  EXPECT_CALL(runtime_, WaitForStdin(_, true, _))
      .WillOnce([](const std::optional<timespec>&, bool, bool* stdin_ready) {
        *stdin_ready = true;
        return 1;
      })
      .WillOnce([](const std::optional<timespec>&, bool, bool* stdin_ready) {
        *stdin_ready = true;
        return 1;
      });
  ExpectPrintCommandThenEof();
  ExpectProcStatSeek();
  ExpectProcStatRead();
  EXPECT_CALL(runtime_, Write(STDOUT_FILENO, _, _))
      .WillOnce([](int, const void* buffer, std::size_t count) {
        EXPECT_EQ(std::string(static_cast<const char*>(buffer), count),
                  "core0=50.00% core1=50.00%\n");
        return static_cast<ssize_t>(count);
      });
  EXPECT_CALL(runtime_, GetMonotonicNow()).Times(0);

  EXPECT_EQ(app_.Run(&error_message_), 0);
  EXPECT_TRUE(error_message_.empty());
}

TEST_F(CpuMonitorAppPrintRunTest, ReturnsErrorWhenPrintWriteFails) {
  InitializeApp();
  ASSERT_TRUE(WriteUpdatedProcStat());

  ExpectNonInteractiveStartup();
  ExpectStdinReady();
  ExpectPrintCommand();
  ExpectProcStatSeek();
  ExpectProcStatRead();
  EXPECT_CALL(runtime_, Write(STDOUT_FILENO, _, _))
      .WillOnce([](int, const void*, std::size_t) {
        errno = EPIPE;
        return -1;
      });
  EXPECT_CALL(runtime_, GetMonotonicNow()).Times(0);

  EXPECT_EQ(app_.Run(&error_message_), 1);
  EXPECT_EQ(error_message_, "write() failed.");
}

TEST_F(CpuMonitorAppPrintRunTest, ReturnsErrorWhenPrintCannotSeekProcStat) {
  InitializeApp();

  ExpectNonInteractiveStartup();
  ExpectStdinReady();
  ExpectPrintCommand();
  EXPECT_CALL(runtime_, Seek(proc_fd_, 0, SEEK_SET)).WillOnce(Return(-1));
  EXPECT_CALL(runtime_, Read(proc_fd_, _, _)).Times(0);
  EXPECT_CALL(runtime_, Write(_, _, _)).Times(0);
  EXPECT_CALL(runtime_, GetMonotonicNow()).Times(0);

  EXPECT_EQ(app_.Run(&error_message_), 1);
  EXPECT_EQ(error_message_, "Unable to seek /proc/stat.");
}

TEST_F(CpuMonitorAppPrintRunTest, ReturnsErrorWhenPrintCannotReadProcStat) {
  InitializeApp();

  ExpectNonInteractiveStartup();
  ExpectStdinReady();
  ExpectPrintCommand();
  EXPECT_CALL(runtime_, Seek(proc_fd_, 0, SEEK_SET)).WillOnce(Return(0));
  EXPECT_CALL(runtime_, Read(proc_fd_, _, _))
      .WillOnce([](int, void*, std::size_t) {
        errno = EIO;
        return -1;
      });
  EXPECT_CALL(runtime_, Write(_, _, _)).Times(0);
  EXPECT_CALL(runtime_, GetMonotonicNow()).Times(0);

  EXPECT_EQ(app_.Run(&error_message_), 1);
  EXPECT_EQ(error_message_, "Unable to read /proc/stat.");
}

TEST_F(CpuMonitorAppScheduledLogRunTest, WritesScheduledLogWhenDeadlineIsReached) {
  InitializeApp();
  ASSERT_TRUE(WriteUpdatedProcStat());

  ExpectNonInteractiveStartup();
  EXPECT_CALL(runtime_, GetMonotonicNow())
      .WillOnce(Return(std::optional<timespec>{timespec{15, 20}}))
      .WillOnce(Return(std::optional<timespec>{timespec{15, 20}}))
      .WillOnce(Return(std::optional<timespec>{timespec{16, 20}}))
      .WillOnce(Return(std::optional<timespec>{timespec{16, 20}}));
  EXPECT_CALL(runtime_, WaitForStdin(_, true, _))
      .WillOnce([](const std::optional<timespec>& timeout, bool,
                   bool* stdin_ready) {
        EXPECT_TRUE(timeout.has_value());
        if (timeout.has_value()) {
          EXPECT_EQ(timeout->tv_sec, 0);
          EXPECT_EQ(timeout->tv_nsec, 0);
        }
        *stdin_ready = false;
        return 0;
      })
      .WillOnce([](const std::optional<timespec>& timeout, bool,
                   bool* stdin_ready) {
        EXPECT_TRUE(timeout.has_value());
        if (timeout.has_value()) {
          EXPECT_EQ(timeout->tv_sec, 4);
          EXPECT_EQ(timeout->tv_nsec, 0);
        }
        *stdin_ready = true;
        return 1;
      });
  ExpectProcStatSeek();
  ExpectProcStatRead();
  EXPECT_CALL(runtime_, GetLocalTimeNow(_)).WillOnce([](tm* output) {
    *output = {};
    output->tm_year = 126;
    output->tm_mon = 3;
    output->tm_mday = 12;
    output->tm_hour = 3;
    output->tm_min = 4;
    output->tm_sec = 5;
    return true;
  });
  EXPECT_CALL(runtime_, Write(log_fd_, _, _))
      .WillOnce([](int, const void* buffer, std::size_t count) {
        EXPECT_EQ(std::string(static_cast<const char*>(buffer), count),
                  "2026-04-12 03:04:05 core0=50.00% core1=50.00%\n");
        return static_cast<ssize_t>(count);
      });
  EXPECT_CALL(runtime_, Read(STDIN_FILENO, _, _))
      .WillOnce([](int, void* buffer, std::size_t) {
        static constexpr char kQuitCommand[] = "quit\n";
        std::memcpy(buffer, kQuitCommand, sizeof(kQuitCommand) - 1);
        return static_cast<ssize_t>(sizeof(kQuitCommand) - 1);
      });

  EXPECT_EQ(app_.Run(&error_message_), 0);
  EXPECT_TRUE(error_message_.empty());
}

TEST_F(CpuMonitorAppScheduledLogRunTest,
       ReturnsErrorWhenScheduledLogClockReadFails) {
  InitializeApp();

  ExpectNonInteractiveStartup();
  EXPECT_CALL(runtime_, GetMonotonicNow())
      .WillOnce(Return(std::optional<timespec>{timespec{14, 20}}))
      .WillOnce(Return(std::optional<timespec>{}));
  EXPECT_CALL(runtime_, WaitForStdin(_, true, _))
      .WillOnce([](const std::optional<timespec>& timeout, bool,
                   bool* stdin_ready) {
        EXPECT_TRUE(timeout.has_value());
        if (timeout.has_value()) {
          EXPECT_EQ(timeout->tv_sec, 1);
          EXPECT_EQ(timeout->tv_nsec, 0);
        }
        *stdin_ready = false;
        return 0;
      });
  EXPECT_CALL(runtime_, Read(STDIN_FILENO, _, _)).Times(0);
  EXPECT_CALL(runtime_, Read(proc_fd_, _, _)).Times(0);
  EXPECT_CALL(runtime_, Write(_, _, _)).Times(0);

  EXPECT_EQ(app_.Run(&error_message_), 1);
  EXPECT_EQ(error_message_, "Unable to read monotonic clock.");
}

}  // namespace
