#include "close_example.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "tests/unit/close_wrap_test_support.h"

namespace {

using ::testing::Return;

class ScopedCloseMock {
 public:
  explicit ScopedCloseMock(CloseMock* mock) : previous_(SetActiveCloseMock(mock)) {}
  ScopedCloseMock(const ScopedCloseMock&) = delete;
  ScopedCloseMock& operator=(const ScopedCloseMock&) = delete;

  ~ScopedCloseMock() { SetActiveCloseMock(previous_); }

 private:
  CloseMock* previous_;
};

TEST(CloseWrapTest, RoutesCloseThroughMockInsteadOfSystemCall) {
  CloseMock mock;
  ScopedCloseMock scoped_mock(&mock);

  EXPECT_CALL(mock, Close(123)).WillOnce(Return(0));

  EXPECT_EQ(CloseExample(123), 0);
}

TEST(CloseWrapTest, CanSimulateCloseFailure) {
  CloseMock mock;
  ScopedCloseMock scoped_mock(&mock);

  EXPECT_CALL(mock, Close(456)).WillOnce(Return(-1));

  EXPECT_EQ(CloseExample(456), -1);
}

}  // namespace
