"""This module defines a custom rule for generating C++ protobuf libraries."""

load("@com_google_protobuf//bazel/common:proto_info.bzl", "ProtoInfo")

def _cc_proto_library_impl(ctx):
    for target in ctx.attr.deps:
        proto_info = target[ProtoInfo]
        for [source_file, descriptor_set_file] in zip(
            proto_info.transitive_sources.to_list(),
            proto_info.transitive_descriptor_sets.to_list(),
        ):
            generated_h_file = ctx.actions.declare_file(
                source_file.basename.replace(".proto", ".pb.h"),
                sibling = source_file,
            )
            generated_cc_file = ctx.actions.declare_file(
                source_file.basename.replace(".proto", ".pb.cc"),
                sibling = source_file,
            )
            ctx.actions.run(
                inputs = [descriptor_set_file],
                outputs = [generated_h_file, generated_cc_file],
                arguments = ["--file_descriptor_set=" + descriptor_set_file.short_path],
                executable = ctx.executable._generator,
            )

cc_proto_library = rule(
    implementation = _cc_proto_library_impl,
    attrs = {
        "deps": attr.label_list(mandatory = True, providers = [ProtoInfo]),
        "_generator": attr.label(default = "//proto:generate", executable = True, cfg = "exec"),
    },
    output_to_genfiles = True,
)
