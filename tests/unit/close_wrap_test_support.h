#ifndef TESTS_UNIT_CLOSE_WRAP_TEST_SUPPORT_H_
#define TESTS_UNIT_CLOSE_WRAP_TEST_SUPPORT_H_

#include <gmock/gmock.h>

class CloseMock {
 public:
  MOCK_METHOD(int, Close, (int fd));
};

CloseMock* SetActiveCloseMock(CloseMock* mock);

#endif  // TESTS_UNIT_CLOSE_WRAP_TEST_SUPPORT_H_
