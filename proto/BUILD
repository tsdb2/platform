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

cc_test(
    name = "wire_test",
    srcs = ["wire_test.cc"],
    deps = [
        ":wire",
        "//common:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "object",
    hdrs = ["object.h"],
    deps = [
        ":wire",
        "//common:type_string",
        "//common:utilities",
        "//io:cord",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "plugin",
    hdrs = ["plugin.h"],
    deps = [
        ":object",
    ],
)

cc_binary(
    name = "proto_cc",
    srcs = ["proto.cc"],
    deps = [
        ":plugin",
        "//common:utilities",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)
