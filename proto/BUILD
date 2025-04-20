load("@com_google_protobuf//bazel:proto_library.bzl", "proto_library")
load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")
load("//proto:proto.bzl", "tsdb2_cc_proto_library")

package(default_visibility = ["//visibility:private"])

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
    name = "runtime",
    srcs = [
        "duration.pb.sync.cc",
        "reflection.cc",
        "text_format.cc",
        "time_util.cc",
        "timestamp.pb.sync.cc",
        "wire_format.cc",
    ],
    hdrs = [
        "duration.pb.sync.h",
        "proto.h",
        "reflection.h",
        "runtime.h",
        "text_format.h",
        "time_util.h",
        "timestamp.pb.sync.h",
        "wire_format.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":text_writer",
        "//common:flat_map",
        "//common:flat_set",
        "//common:no_destructor",
        "//common:re",
        "//common:to_array",
        "//common:trie_map",
        "//common:utilities",
        "//io:buffer",
        "//io:cord",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:config",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "proto_test",
    srcs = ["proto_test.cc"],
    deps = [
        ":runtime",
        "//common:fingerprint",
        "//common:testing",
        "//io:buffer",
        "//io:buffer_testing",
        "//proto/tests:field_test_cc_proto",
        "@com_google_absl//absl/hash",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "wire_format_test",
    srcs = ["wire_format_test.cc"],
    deps = [
        ":runtime",
        "//common:testing",
        "//common:utilities",
        "//io:buffer",
        "//io:buffer_testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
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

cc_test(
    name = "text_format_test",
    srcs = ["text_format_test.cc"],
    deps = [
        ":runtime",
        "//common:testing",
        "//proto/tests:field_test_cc_proto",
        "//proto/tests:indirection_test_cc_proto",
        "//proto/tests:map_test_cc_proto",
        "//proto/tests:map_type_test_cc_proto",
        "//proto/tests:oneof_test_cc_proto",
        "//proto/tests:time_field_test_cc_proto",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

tsdb2_cc_proto_library(
    name = "descriptor_cc_proto",
    proto = "@com_google_protobuf//:descriptor_proto",
    visibility = ["//visibility:public"],
)

proto_library(
    name = "annotations_proto",
    srcs = ["annotations.proto"],
    visibility = ["//visibility:public"],
    deps = ["@com_google_protobuf//:descriptor_proto"],
)

tsdb2_cc_proto_library(
    name = "annotations_cc_proto",
    proto = ":annotations_proto",
    visibility = ["//visibility:public"],
    deps = [":descriptor_cc_proto"],
)

proto_library(
    name = "dependency_mapping_proto",
    srcs = ["dependency_mapping.proto"],
    deps = [":annotations_proto"],
)

tsdb2_cc_proto_library(
    name = "dependency_mapping_cc_proto",
    proto = ":dependency_mapping_proto",
)

cc_library(
    name = "descriptor",
    srcs = [
        "annotations.pb.sync.cc",
        "dependency_mapping.pb.sync.cc",
        "descriptor.pb.sync.cc",
    ],
    hdrs = [
        "annotations.pb.sync.h",
        "dependency_mapping.pb.sync.h",
        "descriptor.pb.sync.h",
    ],
    deps = [
        ":runtime",
    ],
)

proto_library(
    name = "plugin_proto",
    srcs = ["plugin.proto"],
    visibility = ["//visibility:public"],
    deps = ["@com_google_protobuf//:descriptor_proto"],
)

tsdb2_cc_proto_library(
    name = "plugin_cc_proto",
    proto = ":plugin_proto",
    visibility = ["//visibility:public"],
    deps = [":descriptor_cc_proto"],
)

cc_library(
    name = "plugin",
    srcs = ["plugin.pb.sync.cc"],
    hdrs = ["plugin.pb.sync.h"],
    deps = [
        ":descriptor",
        ":runtime",
    ],
)

cc_library(
    name = "generator",
    srcs = ["generator.cc"],
    hdrs = ["generator.h"],
    deps = [
        ":dependencies",
        ":descriptor",
        ":plugin",
        ":runtime",
        ":text_writer",
        "//common:flat_map",
        "//common:flat_set",
        "//common:no_destructor",
        "//common:re",
        "//common:utilities",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

proto_library(
    name = "timestamp_proto",
    srcs = ["timestamp.proto"],
    visibility = ["//visibility:public"],
)

tsdb2_cc_proto_library(
    name = "timestamp_cc_proto",
    enable_reflection = True,
    # Set the flag below to True only if you need to update the timestamp.pb.sync.* files.
    internal_generate_definitions_for_google_api_types_i_dont_care_about_odr_violations = True,
    proto = ":timestamp_proto",
    use_raw_google_api_types = True,
)

proto_library(
    name = "duration_proto",
    srcs = ["duration.proto"],
    visibility = ["//visibility:public"],
)

tsdb2_cc_proto_library(
    name = "duration_cc_proto",
    enable_reflection = True,
    # Set the flag below to True only if you need to update the duration.pb.sync.* files.
    internal_generate_definitions_for_google_api_types_i_dont_care_about_odr_violations = True,
    proto = ":duration_proto",
    use_raw_google_api_types = True,
)

cc_test(
    name = "generator_test",
    srcs = ["generator_test.cc"],
    deps = [
        ":generator",
        ":runtime",
        "//common:fingerprint",
        "//common:flat_map",
        "//common:testing",
        "//common:trie_map",
        "//common:utilities",
        "//proto/tests:field_test_cc_proto",
        "//proto/tests:indirection_test_cc_proto",
        "//proto/tests:map_test_cc_proto",
        "//proto/tests:map_type_test_cc_proto",
        "//proto/tests:oneof_test_cc_proto",
        "//proto/tests:time_field_test_cc_proto",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "reflection_test",
    srcs = ["reflection_test.cc"],
    deps = [
        ":runtime",
        "//common:testing",
        "//common:utilities",
        "//proto/tests:field_test_cc_proto",
        "//proto/tests:indirection_test_cc_proto",
        "//proto/tests:map_test_cc_proto",
        "//proto/tests:map_type_test_cc_proto",
        "//proto/tests:oneof_test_cc_proto",
        "//proto/tests:time_field_test_cc_proto",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "flag_test",
    srcs = ["flag_test.cc"],
    deps = [
        ":runtime",
        "//common:flag_override",
        "//common:testing",
        "//proto/tests:field_test_cc_proto",
        "@com_google_absl//absl/flags:flag",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_binary(
    name = "generate",
    srcs = ["generate.cc"],
    visibility = ["//visibility:public"],
    deps = [
        ":descriptor",
        ":generator",
        ":runtime",
        "//common:utilities",
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
    name = "protoc_plugin",
    srcs = ["plugin.cc"],
    visibility = ["//visibility:public"],
    deps = [
        ":generator",
        ":plugin",
        "//common:utilities",
        "@com_google_absl//absl/log:initialize",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)
