load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

COMMON_COPTS = [
    "-std=c++17",
    "-Wall",
    "-Wextra",
    "-pedantic",
]

cc_test(
    name = "args_test",
    srcs = ["args_test.cc"],
    copts = COMMON_COPTS,
    deps = [
        "//src:args",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "app_test",
    srcs = ["app_test.cc"],
    copts = COMMON_COPTS,
    deps = [
        "//src:app",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "cpu_reader_test",
    srcs = ["cpu_reader_test.cc"],
    copts = COMMON_COPTS,
    deps = [
        "//src:cpu_reader",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "close_wrap_test_support",
    srcs = ["close_wrap_test_support.cc"],
    hdrs = ["close_wrap_test_support.h"],
    copts = COMMON_COPTS,
    deps = [
        "@googletest//:gmock",
    ],
    alwayslink = True,
)

cc_test(
    name = "close_wrap_test",
    srcs = ["close_wrap_test.cc"],
    copts = COMMON_COPTS,
    linkopts = [
        "-Wl,--wrap=close",
    ],
    deps = [
        ":close_wrap_test_support",
        "//src:close_example",
        "@googletest//:gtest_main",
    ],
)
