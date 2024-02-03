package(default_visibility = ["//visibility:public"])

cc_library(
    name = "crash",
    srcs = ["crash.cc"],
    hdrs = ["crash.h"],
    deps = [
        "//common:flat_map",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/log",
    ],
    alwayslink = True,
)

cc_library(
    name = "init_tsdb2",
    srcs = ["init_tsdb2.cc"],
    hdrs = ["init_tsdb2.h"],
    deps = [
        ":crash",
        "//http:http_node",
        "//net:sockets",
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
