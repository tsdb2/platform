package(default_visibility = ["//visibility:public"])

cc_library(
    name = "buffer",
    hdrs = ["buffer.h"],
    deps = [
        "@com_google_absl//absl/log:check",
    ],
)

cc_test(
    name = "buffer_test",
    srcs = ["buffer_test.cc"],
    deps = [
        ":buffer",
        "//common:testing",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "buffer_testing",
    testonly = True,
    hdrs = ["buffer_testing.h"],
    deps = [
        ":buffer",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "cord",
    srcs = ["cord.cc"],
    hdrs = ["cord.h"],
    deps = [
        ":buffer",
        "@com_google_absl//absl/container:inlined_vector",
    ],
)

cc_test(
    name = "cord_test",
    srcs = ["cord_test.cc"],
    deps = [
        ":buffer_testing",
        ":cord",
        "//common:testing",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "fd",
    hdrs = ["fd.h"],
)

cc_test(
    name = "fd_test",
    srcs = ["fd_test.cc"],
    deps = [
        ":fd",
        "//common:testing",
        "@com_google_absl//absl/hash",
        "@com_google_googletest//:gtest_main",
    ],
)
