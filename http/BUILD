package(default_visibility = ["//visibility:public"])

cc_library(
    name = "http_node",
    srcs = ["http_node.cc"],
    hdrs = [
        "http.h",
        "http_node.h",
    ],
    deps = [
        "//common:buffer",
        "//common:reffed_ptr",
        "//common:utilities",
        "//net:sockets",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)
