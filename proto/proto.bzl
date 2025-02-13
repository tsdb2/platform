"""This module defines a custom rule for generating C++ protobuf libraries."""

load("@com_google_protobuf//bazel/common:proto_info.bzl", "ProtoInfo")
load("@rules_cc//cc:find_cc_toolchain.bzl", "find_cc_toolchain", "use_cc_toolchain")

def _replace_extension(path, extension):
    if path.endswith(".proto"):
        return path[:-6] + extension
    else:
        return path + extension

def _join_path(*paths):
    return "/".join([path.strip("/") for path in paths])

def _cc_proto_library_impl(ctx):
    proto_info = ctx.attr.proto[ProtoInfo]
    cc_infos = [dep[CcInfo] for dep in ctx.attr.deps]
    generated_header_files = []
    generated_source_files = []
    generated_files = []
    for source_file in proto_info.direct_sources:
        generated_header_file = ctx.actions.declare_file(
            _replace_extension(source_file.basename, ".pb.h"),
            sibling = source_file,
        )
        generated_header_files.append(generated_header_file)
        generated_source_file = ctx.actions.declare_file(
            _replace_extension(source_file.basename, ".pb.cc"),
            sibling = source_file,
        )
        generated_source_files.append(generated_source_file)
        generated_files += [generated_header_file, generated_source_file]
    ctx.actions.run(
        inputs = [proto_info.direct_descriptor_set],
        outputs = generated_files,
        arguments = [
            "--root_path=" + _join_path(ctx.genfiles_dir.path, proto_info.proto_source_root),
            "--file_descriptor_sets=" + proto_info.direct_descriptor_set.path,
        ],
        executable = ctx.executable._generator,
    )
    cc_toolchain = find_cc_toolchain(ctx)
    feature_configuration = cc_common.configure_features(
        ctx = ctx,
        cc_toolchain = cc_toolchain,
        language = "c++",
    )
    [compilation_context, compilation_outputs] = cc_common.compile(
        actions = ctx.actions,
        name = ctx.attr.name,
        feature_configuration = feature_configuration,
        cc_toolchain = cc_toolchain,
        srcs = generated_source_files,
        public_hdrs = generated_header_files,
        compilation_contexts = [cc_info.compilation_context for cc_info in cc_infos],
    )
    generated_files = generated_files + compilation_outputs.objects + compilation_outputs.pic_objects
    [linking_context, _] = cc_common.create_linking_context_from_compilation_outputs(
        actions = ctx.actions,
        name = ctx.attr.name,
        feature_configuration = feature_configuration,
        cc_toolchain = cc_toolchain,
        compilation_outputs = compilation_outputs,
    )
    return [
        DefaultInfo(files = depset(generated_files)),
        CcInfo(
            compilation_context = compilation_context,
            linking_context = linking_context,
        ),
    ]

cc_proto_library = rule(
    implementation = _cc_proto_library_impl,
    attrs = {
        "proto": attr.label(mandatory = True, providers = [ProtoInfo]),
        "deps": attr.label_list(providers = [CcInfo]),
        "_generator": attr.label(default = "//proto:generate", executable = True, cfg = "exec"),

        # TODO: remove this attribute when https://github.com/bazelbuild/bazel/issues/7260 is
        # resolved. See instructions at
        # https://github.com/bazelbuild/rules_cc/blob/a1162270a0bb680190e8b4f3dab066f15a1ede6c/cc/find_cc_toolchain.bzl.
        "_cc_toolchain": attr.label(default = "@rules_cc//cc:current_cc_toolchain", cfg = "exec"),
    },
    output_to_genfiles = True,
    toolchains = use_cc_toolchain(),
    fragments = ["cpp"],
)
