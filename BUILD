load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

COMMON_COPTS = [
    "-std=c++17",
    "-Wall",
    "-Wextra",
    "-pedantic",
]

cc_library(
    name = "args_lib",
    hdrs = ["src/args.h"],
    srcs = ["src/args.cc"],
    copts = COMMON_COPTS,
    includes = ["src"],
)

cc_library(
    name = "cpu_reader_lib",
    hdrs = ["src/cpu_reader.h"],
    srcs = ["src/cpu_reader.cc"],
    copts = COMMON_COPTS,
    includes = ["src"],
)

cc_library(
    name = "app_lib",
    hdrs = ["src/app.h"],
    srcs = ["src/app.cc"],
    copts = COMMON_COPTS,
    includes = ["src"],
    deps = [
        ":args_lib",
        ":cpu_reader_lib",
    ],
)

cc_binary(
    name = "cpu_monitor",
    srcs = [
        "src/main.cc",
    ],
    copts = COMMON_COPTS,
    deps = [":app_lib"],
    visibility = ["//visibility:public"],
)

cc_test(
    name = "args_test",
    srcs = ["tests/unit/args_test.cc"],
    copts = COMMON_COPTS,
    deps = [
        ":args_lib",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "app_test",
    srcs = ["tests/unit/app_test.cc"],
    copts = COMMON_COPTS,
    deps = [
        ":app_lib",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "cpu_reader_test",
    srcs = ["tests/unit/cpu_reader_test.cc"],
    copts = COMMON_COPTS,
    deps = [
        ":cpu_reader_lib",
        "@googletest//:gtest_main",
    ],
)
