load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "base_module",
    hdrs = ["base_module.h"],
    visibility = ["//visibility:private"],
    deps = [
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "module_manager",
    srcs = ["module_manager.cc"],
    hdrs = ["module_manager.h"],
    visibility = ["//visibility:private"],
    deps = [
        ":base_module",
        "//common:singleton",
        "//common:utilities",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_library(
    name = "init_tsdb2",
    srcs = ["init_tsdb2.cc"],
    hdrs = ["init_tsdb2.h"],
    deps = [
        ":base_module",
        ":module_manager",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/debugging:failure_signal_handler",
        "@com_google_absl//absl/debugging:symbolize",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/log:initialize",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "init_tsdb2_test",
    srcs = ["init_tsdb2_test.cc"],
    deps = [
        ":base_module",
        ":init_tsdb2",
        ":module_manager",
        "//common:scoped_override",
        "//common:singleton",
        "//common:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "module",
    hdrs = ["module.h"],
    deps = [
        ":base_module",
        ":init_tsdb2",
        "//common:singleton",
        "//common:utilities",
        "@com_google_absl//absl/status",
    ],
)

cc_test(
    name = "module_test",
    srcs = ["module_test.cc"],
    deps = [
        ":module",
        ":module_manager",
        "//common:scoped_override",
        "//common:singleton",
        "//common:testing",
        "//common:type_string",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "statusz",
    srcs = ["statusz.cc"],
    deps = [
        ":module",
        "//common:utilities",
        "//http",
        "//http:default_server",
        "//http:handlers",
        "//io:buffer",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = True,
)

cc_library(
    name = "flagz",
    srcs = ["flagz.cc"],
    deps = [
        ":module",
        "//common:utilities",
        "//http",
        "//http:default_server",
        "//http:handlers",
        "//io:buffer",
        "@com_google_absl//absl/flags:reflection",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = True,
)

cc_library(
    name = "envz",
    srcs = ["envz.cc"],
    deps = [
        ":module",
        "//common:env",
        "//common:utilities",
        "//http",
        "//http:default_server",
        "//http:handlers",
        "//io:buffer",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = True,
)

cc_library(
    name = "healthz",
    srcs = ["healthz.cc"],
    hdrs = ["healthz.h"],
    deps = [
        ":module",
        "//common:singleton",
        "//common:utilities",
        "//http",
        "//http:default_server",
        "//http:handlers",
        "//io:buffer",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
    alwayslink = True,
)

cc_test(
    name = "healthz_test",
    srcs = ["healthz_test.cc"],
    deps = [
        ":healthz",
        ":testing",
        "//common:scoped_override",
        "//common:singleton",
        "//common:testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "testing",
    testonly = True,
    hdrs = ["testing.h"],
    deps = [
        ":init_tsdb2",
        "//common:testing",
        "@com_google_absl//absl/flags:reflection",
        "@com_google_googletest//:gtest",
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
        ":envz",  # build_cleaner: keep
        ":flagz",  # build_cleaner: keep
        ":healthz",  # build_cleaner: keep
        ":init_tsdb2",
        ":statusz",  # build_cleaner: keep
        "//http:default_server",
    ],
)
