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
        "src/app.h",
        "src/args.h",
        "src/cpu_reader.h",
    ],
    srcs = [
        "src/app.cc",
        "src/args.cc",
        "src/cpu_reader.cc",
    ],
    copts = COMMON_COPTS,
    includes = ["src"],
)

cc_binary(
    name = "cpu_monitor",
    srcs = [
        "src/main.cc",
    ],
    copts = COMMON_COPTS,
    deps = [":cpu_monitor_lib"],
    visibility = ["//visibility:public"],
)

cc_test(
    name = "args_test",
    srcs = ["tests/unit/args_test.cc"],
    copts = COMMON_COPTS,
    deps = [
        ":cpu_monitor_lib",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "app_test",
    srcs = ["tests/unit/app_test.cc"],
    copts = COMMON_COPTS,
    deps = [
        ":cpu_monitor_lib",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "cpu_reader_test",
    srcs = ["tests/unit/cpu_reader_test.cc"],
    copts = COMMON_COPTS,
    deps = [
        ":cpu_monitor_lib",
        "@googletest//:gtest_main",
    ],
)
