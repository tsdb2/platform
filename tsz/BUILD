load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

package(default_visibility = ["//visibility:private"])

cc_library(
    name = "bucketer",
    srcs = ["bucketer.cc"],
    hdrs = ["bucketer.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//common:lock_free_hash_set",
        "//common:no_destructor",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_test(
    name = "bucketer_test",
    srcs = ["bucketer_test.cc"],
    deps = [
        ":bucketer",
        "//common:testing",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "distribution",
    srcs = ["distribution.cc"],
    hdrs = ["distribution.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":bucketer",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "distribution_testing",
    testonly = True,
    hdrs = ["distribution_testing.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":bucketer",
        ":distribution",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "distribution_test",
    srcs = ["distribution_test.cc"],
    deps = [
        ":bucketer",
        ":distribution",
        "//common:testing",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "realm",
    srcs = ["realm.cc"],
    hdrs = ["realm.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//common:no_destructor",
        "//common:reffed_ptr",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest_prod",
    ],
)

cc_test(
    name = "realm_test",
    srcs = ["realm_test.cc"],
    deps = [
        ":realm",
        "//common:reffed_ptr",
        "//common:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "types",
    hdrs = ["types.h"],
    visibility = [":__subpackages__"],
    deps = [
        ":bucketer",
        ":distribution",
        ":realm",
        "//common:flat_map",
        "//common:no_destructor",
        "//common:reffed_ptr",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "types_test",
    srcs = ["types_test.cc"],
    deps = [
        ":types",
        "//common:testing",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "base",
    hdrs = [
        "base.h",
        "coercion.h",
        "field_descriptor.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":types",
        "//common:fixed",
        "//common:flat_map",
        "//common:type_string",
        "//common:utilities",
    ],
)

cc_test(
    name = "base_test",
    srcs = [
        "coercion_test.cc",
        "field_descriptor_test.cc",
    ],
    deps = [
        ":base",
        "//common:testing",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "cell_reader",
    testonly = True,
    hdrs = ["cell_reader.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":base",
        "//common:fixed",
        "//tsz/internal:exporter",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "cell_reader_test",
    srcs = ["cell_reader_test.cc"],
    deps = [
        ":base",
        ":cell_reader",
        "//common:testing",
        "//tsz/internal:exporter",
        "//tsz/internal:shard",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "entity",
    srcs = ["entity.cc"],
    hdrs = ["entity.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":base",
        "//common:no_destructor",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "entity_test",
    srcs = ["entity_test.cc"],
    deps = [
        ":entity",
        "//common:fingerprint",
        "//common:reffed_ptr",
        "//common:testing",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "base_metric",
    hdrs = ["base_metric.h"],
    deps = [
        ":base",
        ":entity",
        "//common:fixed",
        "//common:lazy",
        "//common:reffed_ptr",
        "//tsz/internal:exporter",
        "//tsz/internal:scoped_metric_proxy",
        "//tsz/internal:shard",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "counter",
    hdrs = ["counter.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":base",
        ":base_metric",
    ],
)

cc_test(
    name = "counter_test",
    srcs = ["counter_test.cc"],
    deps = [
        ":counter",
        ":entity",
        "//common:testing",
        "//tsz:cell_reader",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "event_metric",
    hdrs = ["event_metric.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":base",
        ":base_metric",
    ],
)

cc_test(
    name = "event_metric_test",
    srcs = ["event_metric_test.cc"],
    deps = [
        ":entity",
        ":event_metric",
        "//common:testing",
        "//tsz:cell_reader",
        "//tsz:distribution_testing",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "latency_recorder",
    hdrs = ["latency_recorder.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":base",
        ":event_metric",
        "//common:clock",
        "//common:singleton",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_prod",
    ],
)

cc_test(
    name = "latency_recorder_test",
    srcs = ["latency_recorder_test.cc"],
    deps = [
        ":base",
        ":cell_reader",
        ":distribution_testing",
        ":entity",
        ":event_metric",
        ":latency_recorder",
        "//common:mock_clock",
        "//common:no_destructor",
        "//common:scoped_override",
        "//common:testing",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "metric",
    hdrs = ["metric.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":base",
        ":base_metric",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "metric_test",
    srcs = ["metric_test.cc"],
    deps = [
        ":entity",
        ":metric",
        "//common:testing",
        "//tsz:cell_reader",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
