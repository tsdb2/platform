package(default_visibility = ["//visibility:private"])

cc_library(
    name = "http",
    srcs = ["http.cc"],
    hdrs = ["http.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//common:flat_map",
        "//common:utilities",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/flags:flag",
    ],
)

cc_library(
    name = "huffman",
    srcs = ["huffman.cc"],
    hdrs = ["huffman.h"],
    deps = [
        "//io:buffer",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "huffman_test",
    srcs = ["huffman_test.cc"],
    deps = [
        ":huffman",
        "//common:testing",
        "//io:buffer_testing",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "hpack",
    srcs = ["hpack.cc"],
    hdrs = ["hpack.h"],
    deps = [
        ":http",
        ":huffman",
        "//common:flat_map",
        "//common:utilities",
        "//io:buffer",
        "//io:cord",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "hpack_test",
    srcs = ["hpack_test.cc"],
    deps = [
        ":hpack",
        "//common:testing",
        "//io:buffer_testing",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "write_queue",
    srcs = ["write_queue.cc"],
    hdrs = ["write_queue.h"],
    deps = [
        ":http",
        "//net:base_sockets",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "write_queue_test",
    srcs = ["write_queue_test.cc"],
    deps = [
        ":write_queue",
        "//common:default_scheduler",
        "//common:mock_clock",
        "//common:reffed_ptr",
        "//common:scheduler",
        "//common:scoped_override",
        "//common:simple_condition",
        "//common:singleton",
        "//common:utilities",
        "//io:buffer_testing",
        "//net:base_sockets",
        "//net:sockets",
        "//net:ssl_sockets",
        "//net:ssl_testing",
        "//server:testing",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "channel",
    srcs = ["processor.cc"],
    hdrs = [
        "channel.h",
        "processor.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":hpack",
        ":http",
        ":write_queue",
        "//common:reffed_ptr",
        "//common:simple_condition",
        "//common:utilities",
        "//net:base_sockets",
        "//net:epoll_server",
        "//net:sockets",
        "//net:ssl_sockets",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "channel_test",
    srcs = ["channel_test.cc"],
    deps = [
        ":channel",
        ":http",
        "//common:default_scheduler",
        "//common:mock_clock",
        "//common:reffed_ptr",
        "//common:scheduler",
        "//common:scoped_override",
        "//common:simple_condition",
        "//common:singleton",
        "//common:utilities",
        "//io:buffer_testing",
        "//net:base_sockets",
        "//net:sockets",
        "//net:ssl_sockets",
        "//net:ssl_testing",
        "//server:testing",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "server",
    srcs = ["server.cc"],
    hdrs = ["server.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":channel",
        ":http",
        "//common:reffed_ptr",
        "//common:simple_condition",
        "//common:trie_map",
        "//common:utilities",
        "//net:base_sockets",
        "//net:sockets",
        "//net:ssl_sockets",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)
