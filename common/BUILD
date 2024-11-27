package(default_visibility = ["//visibility:public"])

cc_library(
    name = "to_array",
    hdrs = ["to_array.h"],
)

cc_library(
    name = "utilities",
    hdrs = ["utilities.h"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_test(
    name = "utilities_test",
    srcs = ["utilities_test.cc"],
    deps = [
        ":testing",
        ":utilities",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "stacktrace",
    srcs = ["stacktrace.cc"],
    hdrs = ["stacktrace.h"],
    deps = [
        "@com_google_absl//absl/debugging:stacktrace",
        "@com_google_absl//absl/debugging:symbolize",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "lazy",
    hdrs = ["lazy.h"],
    deps = [
        ":utilities",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:bind_front",
    ],
)

cc_test(
    name = "lazy_test",
    srcs = ["lazy_test.cc"],
    deps = [
        ":lazy",
        ":testing",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "flag_override",
    testonly = True,
    hdrs = ["flag_override.h"],
    deps = [
        "@com_google_absl//absl/flags:flag",
    ],
)

cc_library(
    name = "promise",
    hdrs = ["promise.h"],
    deps = [
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "promise_test",
    srcs = ["promise_test.cc"],
    deps = [
        ":mock_clock",
        ":promise",
        ":scheduler",
        ":testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "env",
    srcs = ["env.cc"],
    hdrs = ["env.h"],
    deps = [
        ":no_destructor",
        ":re",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "env_test",
    srcs = ["env_test.cc"],
    deps = [
        ":env",
        ":testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "stats_counter",
    hdrs = ["stats_counter.h"],
)

cc_test(
    name = "stats_counter_test",
    srcs = ["stats_counter_test.cc"],
    deps = [
        ":stats_counter",
        ":testing",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "type_string",
    hdrs = ["type_string.h"],
)

cc_test(
    name = "type_string_test",
    srcs = ["type_string_test.cc"],
    deps = [
        ":testing",
        ":type_string",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "mutex_lock",
    hdrs = ["mutex_lock.h"],
    deps = [
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "mutex_lock_test",
    srcs = ["mutex_lock_test.cc"],
    deps = [
        ":mutex_lock",
        ":testing",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "multi_mutex_lock",
    hdrs = ["multi_mutex_lock.h"],
    deps = [
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "multi_mutex_lock_test",
    srcs = ["multi_mutex_lock_test.cc"],
    deps = [
        ":multi_mutex_lock",
        ":testing",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "fixed",
    hdrs = ["fixed.h"],
)

cc_library(
    name = "testing",
    testonly = True,
    srcs = ["testing.cc"],
    hdrs = ["testing.h"],
    deps = [
        "@com_google_absl//absl/debugging:failure_signal_handler",
        "@com_google_absl//absl/log:initialize",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest",
    ],
    alwayslink = True,
)

cc_library(
    name = "flat_container_internal",
    hdrs = ["flat_container_internal.h"],
    visibility = ["//visibility:private"],
)

cc_library(
    name = "flat_container_testing",
    testonly = True,
    hdrs = ["flat_container_testing.h"],
    visibility = ["//visibility:private"],
    deps = [
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "flat_set",
    hdrs = ["flat_set.h"],
    deps = [
        ":flat_container_internal",
        ":to_array",
        "@com_google_absl//absl/base",
    ],
)

cc_test(
    name = "flat_set_test",
    srcs = ["flat_set_test.cc"],
    deps = [
        ":fingerprint",
        ":fixed",
        ":flat_container_testing",
        ":flat_set",
        ":testing",
        "@com_google_absl//absl/container:inlined_vector",
        "@com_google_absl//absl/hash",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "flat_map",
    hdrs = ["flat_map.h"],
    deps = [
        ":flat_container_internal",
        ":to_array",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/log",
    ],
)

cc_test(
    name = "flat_map_test",
    srcs = ["flat_map_test.cc"],
    deps = [
        ":fingerprint",
        ":flat_container_testing",
        ":flat_map",
        ":testing",
        "@com_google_absl//absl/container:inlined_vector",
        "@com_google_absl//absl/hash",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "lock_free_container_internal",
    hdrs = ["lock_free_container_internal.h"],
    visibility = ["//visibility:private"],
    deps = ["@com_google_absl//absl/hash"],
)

cc_library(
    name = "raw_lock_free_hash",
    hdrs = ["raw_lock_free_hash.h"],
    visibility = ["//visibility:private"],
    deps = [
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "raw_lock_free_hash_test",
    srcs = ["raw_lock_free_hash_test.cc"],
    deps = [
        ":testing",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "lock_free_hash_set",
    hdrs = ["lock_free_hash_set.h"],
    deps = [
        ":lock_free_container_internal",
        ":raw_lock_free_hash",
    ],
)

cc_test(
    name = "lock_free_hash_set_test",
    srcs = ["lock_free_hash_set_test.cc"],
    deps = [
        ":lock_free_hash_set",
        ":testing",
        "@com_google_absl//absl/hash",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "lock_free_hash_map",
    hdrs = ["lock_free_hash_map.h"],
    deps = [
        ":lock_free_container_internal",
        ":raw_lock_free_hash",
    ],
)

cc_test(
    name = "lock_free_hash_map_test",
    srcs = ["lock_free_hash_map_test.cc"],
    deps = [
        ":lock_free_hash_map",
        ":testing",
        "@com_google_absl//absl/hash",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "re",
    srcs = ["re.cc"],
    hdrs = ["re.h"],
    deps = [
        ":reffed_ptr",
        ":utilities",
        "//common/re:automaton",
        "//common/re:parser",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "re_testing",
    testonly = True,
    hdrs = ["re_testing.h"],
    deps = [
        ":re",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "re_testing_test",
    srcs = ["re_testing_test.cc"],
    deps = [
        ":re_testing",
        ":testing",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "re_test",
    srcs = ["re_test.cc"],
    deps = [
        ":re",
        ":testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "raw_trie",
    hdrs = ["raw_trie.h"],
    visibility = ["//visibility:private"],
    deps = [
        ":flat_map",
        ":re",
        "//common/re:automaton",
        "//common/re:dfa",
        "//common/re:nfa",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "trie_set",
    hdrs = ["trie_set.h"],
    deps = [
        ":raw_trie",
        ":re",
        "@com_google_absl//absl/base",
    ],
)

cc_test(
    name = "trie_set_test",
    srcs = ["trie_set_test.cc"],
    deps = [
        ":fingerprint",
        ":re",
        ":testing",
        ":trie_set",
        "@com_google_absl//absl/hash",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "trie_map",
    hdrs = ["trie_map.h"],
    deps = [
        ":raw_trie",
        ":re",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/log",
    ],
)

cc_test(
    name = "trie_map_test",
    srcs = ["trie_map_test.cc"],
    deps = [
        ":fingerprint",
        ":re",
        ":testing",
        ":trie_map",
        "@com_google_absl//absl/hash",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "fingerprint",
    srcs = ["fingerprint.cc"],
    hdrs = ["fingerprint.h"],
    deps = [
        "//common:reffed_ptr",
        "//common:to_array",
        "//common:utilities",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/container:node_hash_set",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "fingerprint_test",
    srcs = ["fingerprint_test.cc"],
    deps = [
        ":fingerprint",
        ":flat_map",
        ":flat_set",
        ":ref_count",
        ":reffed_ptr",
        ":testing",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/container:inlined_vector",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/container:node_hash_set",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "json",
    srcs = ["json.cc"],
    hdrs = ["json.h"],
    deps = [
        ":flat_map",
        ":flat_set",
        ":to_array",
        ":type_string",
        ":utilities",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/container:node_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "json_test",
    srcs = ["json_test.cc"],
    deps = [
        ":flat_map",
        ":flat_set",
        ":json",
        ":testing",
        ":type_string",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/container:node_hash_set",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "reffed_ptr",
    hdrs = ["reffed_ptr.h"],
)

cc_test(
    name = "reffed_ptr_test",
    srcs = ["reffed_ptr_test.cc"],
    deps = [
        ":reffed_ptr",
        ":testing",
        "@com_google_absl//absl/hash",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "ref_count",
    hdrs = ["ref_count.h"],
    deps = [
        "@com_google_absl//absl/log:check",
    ],
)

cc_test(
    name = "ref_count_test",
    srcs = ["ref_count_test.cc"],
    deps = [
        ":ref_count",
        ":reffed_ptr",
        ":testing",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "sequence_number",
    hdrs = ["sequence_number.h"],
)

cc_test(
    name = "sequence_number_test",
    srcs = ["sequence_number_test.cc"],
    deps = [
        ":sequence_number",
        ":testing",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "simple_condition",
    hdrs = ["simple_condition.h"],
    deps = [
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "simple_condition_test",
    srcs = ["simple_condition_test.cc"],
    deps = [
        ":simple_condition",
        ":testing",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "blocking_ref_counted",
    hdrs = ["blocking_ref_counted.h"],
    deps = [
        ":reffed_ptr",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "blocking_ref_counted_test",
    srcs = ["blocking_ref_counted_test.cc"],
    deps = [
        ":blocking_ref_counted",
        ":reffed_ptr",
        ":testing",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "overridable",
    hdrs = ["overridable.h"],
    deps = [
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "overridable_test",
    srcs = ["overridable_test.cc"],
    deps = [
        ":overridable",
        ":testing",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "no_destructor",
    hdrs = ["no_destructor.h"],
)

cc_test(
    name = "no_destructor_test",
    srcs = ["no_destructor_test.cc"],
    deps = [
        ":no_destructor",
        ":testing",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "singleton",
    hdrs = ["singleton.h"],
    deps = [
        ":no_destructor",
        ":utilities",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "singleton_test",
    srcs = ["singleton_test.cc"],
    deps = [
        ":scoped_override",
        ":singleton",
        ":testing",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "scoped_override",
    testonly = True,
    hdrs = ["scoped_override.h"],
)

cc_test(
    name = "scoped_override_test",
    srcs = ["scoped_override_test.cc"],
    deps = [
        ":overridable",
        ":scoped_override",
        ":testing",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "clock",
    srcs = ["clock.cc"],
    hdrs = ["clock.h"],
    deps = [
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "clock_test",
    srcs = ["clock_test.cc"],
    deps = [
        ":clock",
        ":testing",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "mock_clock",
    testonly = True,
    srcs = ["mock_clock.cc"],
    hdrs = ["mock_clock.h"],
    deps = [
        ":clock",
        ":flat_map",
        ":simple_condition",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "mock_clock_test",
    srcs = ["mock_clock_test.cc"],
    deps = [
        ":mock_clock",
        ":testing",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "scheduler",
    srcs = ["scheduler.cc"],
    hdrs = ["scheduler.h"],
    deps = [
        ":clock",
        ":sequence_number",
        ":simple_condition",
        "@com_google_absl//absl/algorithm:container",
        "@com_google_absl//absl/container:node_hash_set",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "scheduler_test",
    srcs = ["scheduler_test.cc"],
    deps = [
        ":mock_clock",
        ":scheduler",
        ":simple_condition",
        ":testing",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "default_scheduler",
    srcs = ["default_scheduler.cc"],
    hdrs = ["default_scheduler.h"],
    deps = [
        ":scheduler",
        ":singleton",
        "@com_google_absl//absl/flags:flag",
    ],
)

cc_library(
    name = "periodic_thread",
    hdrs = ["periodic_thread.h"],
    deps = [
        ":clock",
        ":scheduler",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "periodic_thread_test",
    srcs = ["periodic_thread_test.cc"],
    deps = [
        ":mock_clock",
        ":periodic_thread",
        ":testing",
        "@com_google_absl//absl/status:status_matchers",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "sleep",
    srcs = ["sleep.cc"],
    hdrs = ["sleep.h"],
    deps = [
        ":default_scheduler",
        ":promise",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "sleep_test",
    srcs = ["sleep_test.cc"],
    deps = [
        ":default_scheduler",
        ":mock_clock",
        ":scheduler",
        ":scoped_override",
        ":singleton",
        ":sleep",
        ":testing",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
