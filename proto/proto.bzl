"""This module defines a custom rule for generating C++ protobuf libraries."""

load("@com_google_protobuf//bazel/common:proto_info.bzl", "ProtoInfo")

_COMMON_SUFFIXES = [
    "_cc_proto",
    "_proto",
    "_cc_pb",
    "_pb",
]

def _cc_proto_library_impl(ctx):
    name = ctx.attr.name
    for suffix in _COMMON_SUFFIXES:
        if name.endswith(suffix) and len(name) > len(suffix):
            name = name[:-len(suffix)]
            break
    for target in ctx.attr.deps:
        proto_info = target[ProtoInfo]
        for descriptor in proto_info.transitive_descriptor_sets.to_list():
            # TODO
            pass

cc_proto_library = rule(
    implementation = _cc_proto_library_impl,
    attrs = {
        "deps": attr.label_list(mandatory = True, providers = [ProtoInfo]),
    },
    output_to_genfiles = True,
)
