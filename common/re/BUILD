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
