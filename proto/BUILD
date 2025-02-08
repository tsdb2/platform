package(default_visibility = ["//visibility:private"])

cc_library(
    name = "wire_format",
    srcs = ["wire_format.cc"],
    hdrs = ["wire_format.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//common:utilities",
        "//io:buffer",
        "//io:cord",
        "@com_google_absl//absl/base:config",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "wire_format_test",
    srcs = ["wire_format_test.cc"],
    deps = [
        ":wire_format",
        "//common:testing",
        "//io:buffer_testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "object",
    hdrs = ["object.h"],
    deps = [
        ":wire_format",
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
    name = "dependencies",
    srcs = ["dependencies.cc"],
    hdrs = ["dependencies.h"],
    deps = [
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/container:inlined_vector",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "dependencies_test",
    srcs = ["dependencies_test.cc"],
    deps = [
        ":dependencies",
        "//common:testing",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "file_writer",
    srcs = ["file_writer.cc"],
    hdrs = ["file_writer.h"],
    deps = [
        "@com_google_absl//absl/strings:cord",
    ],
)

cc_test(
    name = "file_writer_test",
    srcs = ["file_writer_test.cc"],
    deps = [
        ":file_writer",
        "//common:testing",
        "@com_google_googletest//:gtest_main",
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
        ":dependencies",
        ":file_writer",
        ":plugin",
        "//common:utilities",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)
