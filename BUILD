load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

COMMON_COPTS = [
    "-std=c++17",
    "-Wall",
    "-Wextra",
    "-pedantic",
]

cc_library(
    name = "cpu_monitor_lib",
    hdrs = [
        "app.h",
        "args.h",
        "cpu_reader.h",
    ],
    srcs = [
        "app.cc",
        "args.cc",
        "cpu_reader.cc",
    ],
    copts = COMMON_COPTS,
)

cc_binary(
    name = "cpu_monitor",
    srcs = [
        "main.cc",
    ],
    copts = COMMON_COPTS,
    deps = [":cpu_monitor_lib"],
    visibility = ["//visibility:public"],
)

cc_test(
    name = "args_test",
    srcs = ["args_test.cc"],
    copts = COMMON_COPTS,
    deps = [
        ":cpu_monitor_lib",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "app_test",
    srcs = ["app_test.cc"],
    copts = COMMON_COPTS,
    deps = [
        ":cpu_monitor_lib",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "cpu_reader_test",
    srcs = ["cpu_reader_test.cc"],
    copts = COMMON_COPTS,
    deps = [
        ":cpu_monitor_lib",
        "@googletest//:gtest_main",
    ],
)
