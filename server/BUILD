cc_library(
    name = "init_tsdb2",
    srcs = ["init_tsdb2.cc"],
    hdrs = ["init_tsdb2.h"],
    deps = [
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_library(
    name = "server_main",
    srcs = [
        "server_main.cc",
        ":init_tsdb2",
    ],
    deps = [
        ":init_tsdb2",
        "@com_google_absl//absl/flags:flag",
    ],
)
