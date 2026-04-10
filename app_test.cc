#include "app.h"

#include <cerrno>
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
  MOCK_METHOD(bool, Initialize, (const AppConfig& config, std::string* error),
              (override));
  MOCK_METHOD(int, Run, (std::string* error), (override));
};

class MockAppRuntime : public AppRuntime {
 public:
  MOCK_METHOD(long, GetOnlineCpuCount, (), (const, override));
  MOCK_METHOD(int, OpenProcStat, (), (const, override));
  MOCK_METHOD(int, OpenOutputFile, (const char* path), (const, override));
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

TEST(CpuMonitorAppMainTest, HelpFlagPrintsUsageWithoutInitializing) {
  MockCpuMonitorApp app;
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
  MockCpuMonitorApp app;
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
  MockCpuMonitorApp app;
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
  MockCpuMonitorApp app;
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
  MockCpuMonitorApp app;
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
  MockCpuMonitorApp app;
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
  MockAppRuntime runtime;
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
  MockAppRuntime runtime;
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
  MockAppRuntime runtime;
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

  MockAppRuntime runtime;
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

  MockAppRuntime runtime;
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

  MockAppRuntime runtime;
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

  MockAppRuntime runtime;
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

  MockAppRuntime runtime;
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

  EXPECT_TRUE(app.Initialize(config, &error_message));
  EXPECT_TRUE(error_message.empty());
}

TEST(CpuMonitorAppRunTest, ReturnsErrorWhenCalledBeforeInitialize) {
  MockAppRuntime runtime;
  TestableCpuMonitorApp app(runtime);
  std::string error_message;

  EXPECT_EQ(app.Run(&error_message), 1);
  EXPECT_EQ(error_message, "Application entered Run before Init completed.");
}

}  // namespace
