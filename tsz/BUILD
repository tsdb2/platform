cc_library(
    name = "base",
    hdrs = [
        "base.h",
        "base_metric.h",
        "coercion.h",
        "field_descriptor.h",
    ],
    deps = [
        "//common:fixed",
        "//common:type_string",
        "//common:utilities",
    ],
)

cc_library(
    name = "counter",
    hdrs = ["counter.h"],
    deps = [
        ":base",
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)

cc_test(
    name = "counter_test",
    srcs = ["counter_test.cc"],
    deps = [
        ":counter",
        "//common:no_destructor",
        "//common:testing",
        "@com_google_googletest//:gtest_main",
    ],
)
