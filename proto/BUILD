package(default_visibility = ["//visibility:private"])

cc_library(
    name = "wire",
    srcs = ["wire.cc"],
    hdrs = ["wire.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//common:utilities",
        "@com_google_absl//absl/base:config",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "plugin",
    srcs = ["plugin.cc"],
    hdrs = ["plugin.h"],
    deps = [
        ":wire",
        "//io:cord",
        "@com_google_absl//absl/types:span",
    ],
)

cc_binary(
    name = "proto_cc",
    srcs = ["proto.cc"],
    deps = [":plugin"],
)
