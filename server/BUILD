package(default_visibility = ["//visibility:public"])

cc_library(
    name = "init_tsdb2",
    srcs = ["init_tsdb2.cc"],
    hdrs = ["init_tsdb2.h"],
    deps = [
        "//http:http_node",
        "//net:sockets",
        "@com_google_absl//absl/debugging:failure_signal_handler",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/log:initialize",
        "@com_google_absl//absl/synchronization",
    ],
)

# TODO: convert to a cc_library.
cc_binary(
    name = "server_main",
    srcs = [
        "server_main.cc",
        ":init_tsdb2",
    ],
    deps = [
        ":init_tsdb2",
        "//http:http_node",
    ],
)
