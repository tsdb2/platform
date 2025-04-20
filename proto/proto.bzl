"""This module defines a custom rule for generating C++ protobuf libraries."""

load("@com_google_protobuf//bazel/common:proto_info.bzl", "ProtoInfo")
load("@rules_cc//cc:find_cc_toolchain.bzl", "find_cc_toolchain", "use_cc_toolchain")
load("@rules_cc//cc/common:cc_common.bzl", "cc_common")
load("@rules_cc//cc/common:cc_info.bzl", "CcInfo")

def _replace_extension(path, extension):
    if path.endswith(".proto"):
        return path[:-6] + extension
    else:
        return path + extension

def _join_path(*paths):
    return "/".join([path.strip("/") for path in paths])

def _make_dependency_mapping(deps):
    mapping = dict()
    for dep in deps:
        (proto_info, cc_info) = (dep[ProtoInfo], dep[CcInfo])
        for proto_source in proto_info.direct_sources:
            proto_source_path = proto_source.short_path
            if proto_source_path not in mapping:
                mapping[proto_source_path] = set()
            for header in cc_info.compilation_context.direct_public_headers:
                mapping[proto_source_path].add(header.short_path)
    return struct(dependency = {
        key: struct(cc_header = [header for header in headers])
        for key, headers in mapping.items()
    })

def _cc_proto_library_impl(ctx):
    proto_info = ctx.attr.proto[ProtoInfo]
    cc_proto_infos = [
        dep[CcInfo]
        for dep in ctx.attr.deps
    ]
    cc_infos = cc_proto_infos + [
        dep[CcInfo]
        for dep in ctx.attr._runtime_deps
    ]
    generated_header_files = []
    generated_source_files = []
    generated_files = []
    for source_file in proto_info.direct_sources:
        generated_header_file = ctx.actions.declare_file(
            _replace_extension(source_file.basename, ".pb.h"),
        )
        generated_header_files.append(generated_header_file)
        generated_source_file = ctx.actions.declare_file(
            _replace_extension(source_file.basename, ".pb.cc"),
        )
        generated_source_files.append(generated_source_file)
        generated_files += [generated_header_file, generated_source_file]
    ctx.actions.run(
        inputs = [proto_info.direct_descriptor_set],
        outputs = generated_files,
        arguments = [
            "--proto_file_descriptor_sets=" + proto_info.direct_descriptor_set.path,
            "--proto_output_directory=" + _join_path(ctx.genfiles_dir.path, ctx.label.package),
            "--proto_dependency_mapping=" + proto.encode_text(_make_dependency_mapping(ctx.attr.deps)),
            "--proto_emit_reflection_api" if ctx.attr.enable_reflection else "--noproto_emit_reflection_api",
            "--proto_use_raw_google_api_types" if ctx.attr.use_raw_google_api_types else "--noproto_use_raw_google_api_types",
            "--proto_internal_generate_definitions_for_google_api_types_i_dont_care_about_odr_violations" if ctx.attr.internal_generate_definitions_for_google_api_types_i_dont_care_about_odr_violations else "--noproto_internal_generate_definitions_for_google_api_types_i_dont_care_about_odr_violations",
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
        linking_contexts = [cc_info.linking_context for cc_info in cc_infos],
    )
    return [
        DefaultInfo(files = depset(generated_files)),
        proto_info,
        CcInfo(
            compilation_context = compilation_context,
            linking_context = linking_context,
        ),
    ]

tsdb2_cc_proto_library = rule(
    implementation = _cc_proto_library_impl,
    attrs = {
        "proto": attr.label(mandatory = True, providers = [ProtoInfo]),
        "enable_reflection": attr.bool(),
        "use_raw_google_api_types": attr.bool(),
        "internal_generate_definitions_for_google_api_types_i_dont_care_about_odr_violations": attr.bool(),
        "deps": attr.label_list(providers = [ProtoInfo, CcInfo]),
        "_generator": attr.label(default = "//proto:generate", executable = True, cfg = "exec"),
        "_runtime_deps": attr.label_list(default = ["//proto:runtime"], providers = [CcInfo]),

        # TODO: remove this attribute when https://github.com/bazelbuild/bazel/issues/7260 is
        # resolved. See instructions at
        # https://github.com/bazelbuild/rules_cc/blob/a1162270a0bb680190e8b4f3dab066f15a1ede6c/cc/find_cc_toolchain.bzl.
        "_cc_toolchain": attr.label(default = "@rules_cc//cc:current_cc_toolchain", cfg = "exec"),
    },
    output_to_genfiles = True,
    toolchains = use_cc_toolchain(),
    fragments = ["cpp"],
)
