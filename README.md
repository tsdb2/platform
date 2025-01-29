# TSDB2 Platform

## Description

This repository provides the "TSDB2 Platform", a comprehensive C++ framework for highly scalable
distributed computing that we use within the [TSDB2][tsdb2] project.

## Dependencies

- Linux kernel
- Clang++ 18 or higher (the code base currently uses C++17; C++2x migration is planned)
- [Bazel][bazel]
- [Abseil][abseil]
- [GoogleTest][googletest]
- [OpenSSL][openssl]
- [Google Protobufs][protobufs] and [Protobuf Compiler][protoc]

Also recommended:

- [VS Code][vscode] with [clangd extension][clangd]
- [comp_db_hook][comp_db_hook] for generating the [compilation database][compilation-database]
  required by clangd
- [LLDB DAP extension][lldb-dap] for debugging with LLDB

## Cloning and Building

The easiest way:

```sh
$ git clone https://github.com/tsdb2/platform.git
$ cd platform
platform$ bazel build ...
```

If you want [clangd][clangd] integration you need to use [comp_db_hook][comp_db_hook] in the build,
which needs a few settings via environment variables. For example if you cloned the repository into
the `/home/user/platform/` directory then you can run:

```sh
/home/user/platform$ bazel build --action_env=COMP_DB_HOOK_COMPILER=clang++-18 --action_env=COMP_DB_HOOK_WORKSPACE_DIR=/home/user/platform ...
```

Tests can be run with `bazel test`, but you might need to override the `TEST_TMPDIR` because its
path might exceed the maximum Unix Domain Socket path length on some systems, preventing some
of our network tests from running. Specifying an explicit test timeout is also recommended, as some
of our tests actively rely on avoiding timeouts to be considered successful. Example:

```sh
/home/user/platform$ bazel test --test_tmpdir=/tmp/ --test_timeout=15s ...
```

## Creating a Platform-Based Project

This project uses the [Bazel build system][bazel], so you can integrate it into your Bazel-based C++
projects by adding a dependency like the following:

```bzl
http_archive(
  name = "tsdb2_platform",
  url = "https://github.com/tsdb2/platform/archive/refs/tags/v1.23.45.zip",
  sha256 = "...",
)
```

Replace the `url` with one of the releases from our [Release page][releases] and set the `sha256`
hash accordingly. You can calculate the hash by running the following:

```sh
$ curl -s -L --output - https://github.com/tsdb2/platform/archive/refs/tags/v1.23.45.zip | sha256sum
```

(Replacing `v1.23.45` with the correct tag.)

NOTE: the `http_archive` rule should work even if you use `MODULE.bazel`. For an example see
https://github.com/tsdb2/comp_db_hook/blob/main/MODULE.bazel.

[abseil]: https://abseil.io/
[bazel]: https://bazel.build/
[clangd]: https://marketplace.visualstudio.com/items?itemName=llvm-vs-code-extensions.vscode-clangd
[comp_db_hook]: https://github.com/tsdb2/comp_db_hook
[compilation-database]: https://clang.llvm.org/docs/JSONCompilationDatabase.html
[googletest]: https://github.com/google/googletest
[lldb-dap]: https://marketplace.visualstudio.com/items?itemName=llvm-vs-code-extensions.lldb-dap
[openssl]: https://openssl-library.org/
[protobufs]: https://protobuf.dev/
[protoc]: https://grpc.io/docs/protoc-installation/
[releases]: https://github.com/tsdb2/platform/releases
[tsdb2]: https://tsdb2.com/
[vscode]: https://code.visualstudio.com/
