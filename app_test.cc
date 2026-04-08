#include "app.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

namespace {

using ::testing::_;
using ::testing::HasSubstr;

class MockCpuMonitorApp : public CpuMonitorApp {
 public:
  MOCK_METHOD(bool, Initialize, (const AppConfig& config, std::string* error),
              (override));
  MOCK_METHOD(int, Run, (std::string* error), (override));
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

}  // namespace
