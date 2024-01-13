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

cc_library(
    name = "select_server",
    srcs = ["select_server.cc"],
    hdrs = ["select_server.h"],
    deps = [
        ":fd",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/synchronization",
    ],
)
