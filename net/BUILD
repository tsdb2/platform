package(default_visibility = ["//visibility:public"])

cc_library(
    name = "sockets",
    srcs = ["sockets.cc"],
    hdrs = ["sockets.h"],
    deps = [
        "//common:buffer",
        "//common:mutex_lock",
        "//common:ref_count",
        "//common:reffed_ptr",
        "//common:utilities",
        "//io:fd",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "sockets_test",
    srcs = ["sockets_test.cc"],
    deps = [
        ":sockets",
        "//common:buffer",
        "//common:reffed_ptr",
        "//common:simple_condition",
        "//common:testing",
        "//common:utilities",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest_main",
    ],
)
