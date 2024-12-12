package(default_visibility = ["//tsz:__subpackages__"])

cc_library(
    name = "metric_config",
    hdrs = ["metric_config.h"],
    deps = [
        "//tsz:types",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "cell",
    srcs = ["cell.cc"],
    hdrs = ["cell.h"],
    deps = [
        "//common:utilities",
        "//tsz:types",
        "@com_google_absl//absl/functional:overload",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "metric",
    srcs = ["metric.cc"],
    hdrs = ["metric.h"],
    deps = [
        ":cell",
        ":metric_config",
        "//common:ref_count",
        "//tsz:types",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "mock_metric_manager",
    testonly = True,
    hdrs = ["mock_metric_manager.h"],
    visibility = ["//visibility:private"],
    deps = [":metric"],
)

cc_test(
    name = "metric_test",
    srcs = ["metric_test.cc"],
    deps = [
        ":metric",
        ":metric_config",
        ":mock_metric_manager",
        ":scoped_metric_proxy",
        ":throw_away_metric_proxy",
        "//common:testing",
        "//tsz:distribution_testing",
        "//tsz:types",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "metric_proxy",
    hdrs = ["metric_proxy.h"],
    deps = [
        ":metric",
        "//tsz:types",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "scoped_metric_proxy",
    hdrs = ["scoped_metric_proxy.h"],
    deps = [
        ":metric",
        ":metric_proxy",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "scoped_metric_proxy_test",
    srcs = ["scoped_metric_proxy_test.cc"],
    deps = [
        ":metric",
        ":metric_config",
        ":metric_proxy",
        ":mock_metric_manager",
        ":scoped_metric_proxy",
        "//common:testing",
        "//tsz:distribution_testing",
        "//tsz:types",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "throw_away_metric_proxy",
    hdrs = ["throw_away_metric_proxy.h"],
    deps = [
        ":metric",
        ":metric_proxy",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "throw_away_metric_proxy_test",
    srcs = ["throw_away_metric_proxy_test.cc"],
    deps = [
        ":metric_config",
        ":mock_metric_manager",
        ":throw_away_metric_proxy",
        "//common:testing",
        "//tsz:distribution_testing",
        "//tsz:types",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "entity",
    srcs = ["entity.cc"],
    hdrs = ["entity.h"],
    deps = [
        ":metric",
        ":metric_config",
        ":scoped_metric_proxy",
        ":throw_away_metric_proxy",
        "//common:ref_count",
        "//common:utilities",
        "//tsz:types",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "mock_entity_manager",
    testonly = True,
    hdrs = ["mock_entity_manager.h"],
    visibility = ["//tsz:__subpackages__"],
    deps = [
        ":entity",
        ":metric_config",
        "//tsz:types",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "entity_test",
    srcs = ["entity_test.cc"],
    deps = [
        ":entity",
        ":metric_config",
        ":mock_entity_manager",
        "//common:testing",
        "//tsz:distribution_testing",
        "//tsz:types",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "entity_proxy",
    hdrs = ["entity_proxy.h"],
    deps = [
        ":entity",
        ":scoped_metric_proxy",
        "//tsz:types",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "entity_proxy_test",
    srcs = ["entity_proxy_test.cc"],
    deps = [
        ":entity",
        ":entity_proxy",
        ":metric_config",
        ":mock_entity_manager",
        ":scoped_metric_proxy",
        "//common:testing",
        "//tsz:distribution_testing",
        "//tsz:types",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "shard",
    srcs = ["shard.cc"],
    hdrs = ["shard.h"],
    deps = [
        ":entity",
        ":entity_proxy",
        ":metric_config",
        ":scoped_metric_proxy",
        "//common:clock",
        "//common:lock_free_hash_map",
        "//common:no_destructor",
        "//common:re",
        "//tsz:types",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "shard_test",
    srcs = ["shard_test.cc"],
    deps = [
        ":metric_config",
        ":shard",
        "//common:testing",
        "//tsz:distribution_testing",
        "//tsz:types",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "exporter",
    srcs = ["exporter.cc"],
    hdrs = ["exporter.h"],
    deps = [
        ":metric_config",
        ":shard",
        "//common:lock_free_hash_map",
        "//common:singleton",
        "//common:utilities",
        "//tsz:types",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_test(
    name = "exporter_test",
    srcs = ["exporter_test.cc"],
    deps = [
        ":exporter",
        "//common:testing",
        "//tsz:base",
        "//tsz:distribution_testing",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
