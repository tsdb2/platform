package(default_visibility = ["//common:__subpackages__"])

cc_library(
    name = "automaton",
    hdrs = ["automaton.h"],
)

cc_library(
    name = "dfa",
    hdrs = ["dfa.h"],
    deps = [
        ":automaton",
        "//common:flat_map",
    ],
)
