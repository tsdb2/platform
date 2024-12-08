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
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_test(
    name = "fd_test",
    srcs = ["fd_test.cc"],
    deps = [
        ":fd",
        "//common:testing",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "sigusr",
    srcs = ["sigusr.cc"],
    hdrs = ["sigusr.h"],
    deps = [
        "//common:ref_count",
        "//common:reffed_ptr",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "sigusr_test",
    srcs = ["sigusr_test.cc"],
    deps = [
        ":sigusr",
        "//common:testing",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "advisory_file_lock",
    srcs = ["advisory_file_lock.cc"],
    hdrs = ["advisory_file_lock.h"],
    deps = [
        ":fd",
        "//common:ref_count",
        "//common:reffed_ptr",
        "//common:utilities",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/container:node_hash_set",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "advisory_file_lock_test",
    srcs = ["advisory_file_lock_test.cc"],
    deps = [
        ":advisory_file_lock",
        ":sigusr",
        "//common:testing",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)
