load("@com_google_protobuf//bazel:proto_library.bzl", "proto_library")
load("//proto:proto.bzl", "tsdb2_cc_proto_library")

package(default_visibility = ["//proto:__subpackages__"])

proto_library(
    name = "field_test_proto",
    testonly = True,
    srcs = ["field_test.proto"],
)

tsdb2_cc_proto_library(
    name = "field_test_cc_proto",
    testonly = True,
    enable_reflection = True,
    proto = ":field_test_proto",
)

proto_library(
    name = "time_field_test_proto",
    testonly = True,
    srcs = ["time_field_test.proto"],
    deps = [
        "//proto:duration_proto",
        "//proto:timestamp_proto",
    ],
)

tsdb2_cc_proto_library(
    name = "time_field_test_cc_proto",
    testonly = True,
    enable_reflection = True,
    proto = ":time_field_test_proto",
)

proto_library(
    name = "oneof_test_proto",
    testonly = True,
    srcs = ["oneof_test.proto"],
    deps = [
        "//proto:duration_proto",
        "//proto:timestamp_proto",
    ],
)

tsdb2_cc_proto_library(
    name = "oneof_test_cc_proto",
    testonly = True,
    enable_reflection = True,
    proto = ":oneof_test_proto",
)

proto_library(
    name = "indirection_test_proto",
    testonly = True,
    srcs = ["indirection_test.proto"],
    deps = [
        "//proto:annotations_proto",
    ],
)

tsdb2_cc_proto_library(
    name = "indirection_test_cc_proto",
    testonly = True,
    enable_reflection = True,
    proto = ":indirection_test_proto",
)

proto_library(
    name = "map_test_proto",
    testonly = True,
    srcs = ["map_test.proto"],
    deps = [
        "//proto:duration_proto",
        "//proto:timestamp_proto",
    ],
)

tsdb2_cc_proto_library(
    name = "map_test_cc_proto",
    testonly = True,
    enable_reflection = True,
    proto = ":map_test_proto",
)

proto_library(
    name = "map_type_test_proto",
    testonly = True,
    srcs = ["map_type_test.proto"],
    deps = ["//proto:annotations_proto"],
)

tsdb2_cc_proto_library(
    name = "map_type_test_cc_proto",
    testonly = True,
    enable_reflection = True,
    proto = ":map_type_test_proto",
)
