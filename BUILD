load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

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
