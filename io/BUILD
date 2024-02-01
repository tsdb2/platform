package(default_visibility = ["//visibility:public"])

cc_library(
    name = "fd",
    hdrs = ["fd.h"],
)

cc_test(
    name = "fd_test",
    srcs = ["fd_test.cc"],
    deps = [
        ":fd",
        "@com_google_absl//absl/hash",
        "@com_google_googletest//:gtest_main",
    ],
)
