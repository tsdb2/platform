"""This module defines a custom rule for generating C++ protobuf libraries."""

load("@com_google_protobuf//bazel/common:proto_info.bzl", "ProtoInfo")

def _replace_extension(path, extension):
    if path.endswith(".proto"):
        return path[:-6] + extension
    else:
        return path + extension

def _join_path(*paths):
    return "/".join([path.strip("/") for path in paths])

def _cc_proto_library_impl(ctx):
    all_generated_files = []
    cc_infos = []
    for target in ctx.attr.deps:
        proto_info = target[ProtoInfo]
        generated_files = []
        for source_file in proto_info.transitive_sources.to_list():
            generated_h_file = ctx.actions.declare_file(
                _replace_extension(source_file.basename, ".pb.h"),
                sibling = source_file,
            )
            generated_cc_file = ctx.actions.declare_file(
                _replace_extension(source_file.basename, ".pb.cc"),
                sibling = source_file,
            )
            generated_files += [generated_h_file, generated_cc_file]
        ctx.actions.run(
            inputs = proto_info.transitive_descriptor_sets,
            outputs = generated_files,
            arguments = [
                "--root_path=" + _join_path(ctx.genfiles_dir.path, proto_info.proto_source_root),
                "--file_descriptor_sets=" + ",".join([
                    file.path
                    for file in proto_info.transitive_descriptor_sets.to_list()
                ]),
            ],
            executable = ctx.executable._generator,
        )
        all_generated_files += generated_files
    return [DefaultInfo(files = depset(all_generated_files))] + cc_infos

cc_proto_library = rule(
    implementation = _cc_proto_library_impl,
    attrs = {
        "deps": attr.label_list(mandatory = True, providers = [ProtoInfo]),
        "_generator": attr.label(default = "//proto:generate", executable = True, cfg = "exec"),
    },
    output_to_genfiles = True,
)
