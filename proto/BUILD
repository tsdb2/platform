load("@com_google_protobuf//bazel:proto_library.bzl", "proto_library")
load("//proto:proto.bzl", "cc_proto_library")

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
    name = "text_writer",
    srcs = ["text_writer.cc"],
    hdrs = ["text_writer.h"],
    deps = [
        "@com_google_absl//absl/strings:cord",
    ],
)

cc_test(
    name = "text_writer_test",
    srcs = ["text_writer_test.cc"],
    deps = [
        ":text_writer",
        "//common:testing",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "temp_descriptor",
    hdrs = ["descriptor.h"],
    deps = [
        ":object",
    ],
)

# proto_library(
#     name = "descriptor_proto",
#     srcs = ["descriptor.proto"],
# )

# cc_proto_library(
#     name = "descriptor_cc_proto",
#     proto = ":descriptor_proto",
#     visibility = ["//visibility:public"],
# )

cc_library(
    name = "generator",
    srcs = ["generator.cc"],
    hdrs = ["generator.h"],
    deps = [
        ":dependencies",
        ":object",
        ":temp_descriptor",
        ":text_writer",
        "//common:flat_map",
        "//common:no_destructor",
        "//common:re",
        "//common:utilities",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "generator_test",
    srcs = ["generator_test.cc"],
    deps = [
        ":generator",
        "//common:testing",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_binary(
    name = "generate",
    srcs = ["generate.cc"],
    visibility = ["//visibility:public"],
    deps = [
        ":generator",
        ":object",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:initialize",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:span",
    ],
)

cc_binary(
    name = "plugin",
    srcs = ["plugin.cc"],
    visibility = ["//visibility:public"],
    deps = [
        ":generator",
        ":object",
        ":temp_descriptor",
        "//common:utilities",
        "@com_google_absl//absl/log:initialize",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)
