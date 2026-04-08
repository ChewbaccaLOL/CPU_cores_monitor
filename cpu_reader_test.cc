#include "cpu_reader.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <iterator>

namespace {

TEST(ParseProcStatBufferTest, ParsesPerCoreLinesAndIgnoresOtherContent) {
  constexpr char kProcStat[] =
      "cpu  100 0 50 400 0 0 0 0\n"
      "cpu0 10 1 2 30 4 5 6 7\n"
      "cpu1 20 3 4 40 5 6 7 8\n"
      "intr 1\n";

  CpuTimes per_core[4];
  std::size_t parsed_core_count = 0;

  const bool ok =
      ParseProcStatBuffer(kProcStat, sizeof(kProcStat) - 1, per_core,
                          std::size(per_core), &parsed_core_count);

  ASSERT_TRUE(ok);
  ASSERT_EQ(parsed_core_count, 2U);
  EXPECT_EQ(per_core[0].user, 10U);
  EXPECT_EQ(per_core[0].idle, 30U);
  EXPECT_EQ(per_core[1].user, 20U);
  EXPECT_EQ(per_core[1].softirq, 7U);
}

TEST(ParseProcStatBufferTest, RejectsInputWhenNoPerCoreLinesExist) {
  constexpr char kProcStat[] =
      "cpu  100 0 50 400 0 0 0 0\n"
      "intr 1\n";

  CpuTimes per_core[2];
  std::size_t parsed_core_count = 0;

  const bool ok =
      ParseProcStatBuffer(kProcStat, sizeof(kProcStat) - 1, per_core,
                          std::size(per_core), &parsed_core_count);

  EXPECT_FALSE(ok);
  EXPECT_EQ(parsed_core_count, 0U);
}

TEST(ParseProcStatBufferTest, RejectsCoreIndexOutsideCapacity) {
  constexpr char kProcStat[] = "cpu2 10 1 2 30 4 5 6 7\n";

  CpuTimes per_core[2];
  std::size_t parsed_core_count = 0;

  const bool ok =
      ParseProcStatBuffer(kProcStat, sizeof(kProcStat) - 1, per_core,
                          std::size(per_core), &parsed_core_count);

  EXPECT_FALSE(ok);
}

TEST(ComputeLoadPercentTest, ComputesActiveTimeRatio) {
  CpuTimes previous;
  previous.user = 10;
  previous.system = 10;
  previous.idle = 60;
  previous.iowait = 10;

  CpuTimes current;
  current.user = 30;
  current.system = 20;
  current.idle = 80;
  current.iowait = 20;

  EXPECT_DOUBLE_EQ(ComputeLoadPercent(previous, current), 50.0);
}

TEST(ComputeLoadPercentTest, ReturnsZeroForNonMonotonicSamples) {
  CpuTimes previous;
  previous.user = 30;
  previous.system = 20;
  previous.idle = 80;
  previous.iowait = 20;

  CpuTimes current;
  current.user = 20;
  current.system = 10;
  current.idle = 70;
  current.iowait = 20;

  EXPECT_DOUBLE_EQ(ComputeLoadPercent(previous, current), 0.0);
}

}  // namespace
