package(default_visibility = ["//common:__subpackages__"])

cc_library(
    name = "automaton",
    hdrs = ["automaton.h"],
)

cc_library(
    name = "dfa",
    srcs = ["dfa.cc"],
    hdrs = ["dfa.h"],
    deps = [
        ":automaton",
        "//common:flat_map",
    ],
)

cc_library(
    name = "nfa",
    srcs = ["nfa.cc"],
    hdrs = ["nfa.h"],
    deps = [
        ":automaton",
        "//common:flat_map",
        "@com_google_absl//absl/container:flat_hash_set",
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
        "@com_google_absl//absl/container:btree",
    ],
)

cc_library(
    name = "parser",
    srcs = ["parser.cc"],
    hdrs = ["parser.h"],
    deps = [
        ":automaton",
        ":temp",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "parser_test",
    srcs = ["parser_test.cc"],
    deps = [
        ":parser",
        ":temp",
        "//common:testing",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)
