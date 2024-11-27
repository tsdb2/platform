package(default_visibility = ["//common:__subpackages__"])

cc_library(
    name = "capture_groups",
    hdrs = ["capture_groups.h"],
)

cc_library(
    name = "automaton",
    srcs = ["automaton.cc"],
    hdrs = ["automaton.h"],
    deps = [
        ":capture_groups",
        "//common:ref_count",
        "@com_google_absl//absl/container:inlined_vector",
    ],
)

cc_library(
    name = "dfa",
    srcs = ["dfa.cc"],
    hdrs = ["dfa.h"],
    deps = [
        ":automaton",
        ":capture_groups",
        "//common:flat_map",
        "@com_google_absl//absl/container:flat_hash_set",
    ],
)

cc_library(
    name = "nfa",
    srcs = ["nfa.cc"],
    hdrs = ["nfa.h"],
    deps = [
        ":automaton",
        ":capture_groups",
        "//common:flat_map",
        "//common:flat_set",
        "@com_google_absl//absl/container:inlined_vector",
    ],
)

cc_library(
    name = "temp",
    srcs = ["temp.cc"],
    hdrs = ["temp.h"],
    deps = [
        ":automaton",
        ":dfa",
        ":nfa",
        "//common:reffed_ptr",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)

cc_library(
    name = "parser",
    srcs = ["parser.cc"],
    hdrs = ["parser.h"],
    deps = [
        ":automaton",
        ":capture_groups",
        ":temp",
        "//common:reffed_ptr",
        "//common:utilities",
        "@com_google_absl//absl/container:inlined_vector",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "re_test",
    srcs = ["re_test.cc"],
    deps = [
        ":automaton",
        ":dfa",
        ":nfa",
        ":parser",
        ":temp",
        "//common:flag_override",
        "//common:reffed_ptr",
        "//common:testing",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)
