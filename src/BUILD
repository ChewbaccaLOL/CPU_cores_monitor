load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

COMMON_COPTS = [
    "-std=c++17",
    "-Wall",
    "-Wextra",
    "-pedantic",
]

package(default_visibility = ["//visibility:private"])

cc_library(
    name = "args",
    hdrs = ["args.h"],
    srcs = ["args.cc"],
    copts = COMMON_COPTS,
    includes = ["."],
    visibility = ["//tests/unit:__pkg__"],
)

cc_library(
    name = "cpu_reader",
    hdrs = ["cpu_reader.h"],
    srcs = ["cpu_reader.cc"],
    copts = COMMON_COPTS,
    includes = ["."],
    visibility = ["//tests/unit:__pkg__"],
)

cc_library(
    name = "app",
    hdrs = ["app.h"],
    srcs = ["app.cc"],
    copts = COMMON_COPTS,
    includes = ["."],
    deps = [
        ":args",
        ":cpu_reader",
    ],
    visibility = ["//tests/unit:__pkg__"],
)

cc_binary(
    name = "cpu_monitor",
    srcs = ["main.cc"],
    copts = COMMON_COPTS,
    deps = [":app"],
    visibility = ["//visibility:public"],
)
