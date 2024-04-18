cc_library(
    name = "base",
    hdrs = [
        "base.h",
        "coercion.h",
        "field_descriptor.h",
        "types.h",
    ],
    deps = [
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
    name = "counter",
    hdrs = ["counter.h"],
    deps = [
        ":base",
        "//common:fixed",
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
