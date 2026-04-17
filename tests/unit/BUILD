load("@rules_cc//cc:defs.bzl", "cc_test")

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
