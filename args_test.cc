#include "args.h"

#include <gtest/gtest.h>

#include <string>

namespace {

TEST(ParseArgumentsTest, ReturnsDefaultsWhenNoFlagsProvided) {
  char program[] = "cpu_monitor";
  char* argv[] = {program};

  const ParseResult result = ParseArguments(1, argv);

  EXPECT_TRUE(result.ok);
  EXPECT_FALSE(result.show_help);
  EXPECT_FALSE(result.config.interval_seconds.has_value());
  EXPECT_FALSE(result.config.output_path.has_value());
  EXPECT_TRUE(result.message.empty());
}

TEST(ParseArgumentsTest, RecognizesHelpFlag) {
  char program[] = "cpu_monitor";
  char help[] = "--help";
  char* argv[] = {program, help};

  const ParseResult result = ParseArguments(2, argv);

  EXPECT_TRUE(result.ok);
  EXPECT_TRUE(result.show_help);
  EXPECT_TRUE(result.message.empty());
}

TEST(ParseArgumentsTest, RejectsInvalidIntervalValue) {
  char program[] = "cpu_monitor";
  char interval_flag[] = "--interval-sec";
  char invalid_value[] = "0";
  char output_flag[] = "--output";
  char output_value[] = "cpu.log";
  char* argv[] = {
      program,
      interval_flag,
      invalid_value,
      output_flag,
      output_value,
  };

  const ParseResult result = ParseArguments(5, argv);

  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.message, "Invalid value for --interval-sec.");
}

TEST(ParseArgumentsTest, RejectsMissingCompanionFlag) {
  char program[] = "cpu_monitor";
  char interval_flag[] = "--interval-sec";
  char interval_value[] = "5";
  char* argv[] = {
      program,
      interval_flag,
      interval_value,
  };

  const ParseResult result = ParseArguments(3, argv);

  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.message,
            "Both --interval-sec and --output must be provided together.");
}

TEST(ParseArgumentsTest, ParsesIntervalAndOutputTogether) {
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

  const ParseResult result = ParseArguments(5, argv);

  ASSERT_TRUE(result.ok);
  ASSERT_TRUE(result.config.interval_seconds.has_value());
  ASSERT_TRUE(result.config.output_path.has_value());
  EXPECT_EQ(*result.config.interval_seconds, 5U);
  EXPECT_EQ(*result.config.output_path, std::string("cpu.log"));
}

}  // namespace
