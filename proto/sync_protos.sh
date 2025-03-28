#!/bin/bash

# exit immediately if a command fails
set -e

# set the cwd to the script's directory
cd "$(dirname "$0")"

# define the source directory inside bazel-bin
SOURCE_DIR=$(realpath ../bazel-bin/proto)
if [ ! -d "$SOURCE_DIR" ]; then
  echo "Error: expected directory '$SOURCE_DIR' does not exist. Did you run 'bazel build'?"
  exit 1
fi

# copy the files
cp "$SOURCE_DIR/descriptor.pb.h" ./descriptor.pb.sync.h
cp "$SOURCE_DIR/descriptor.pb.cc" ./descriptor.pb.sync.cc
cp "$SOURCE_DIR/plugin.pb.h" ./plugin.pb.sync.h
cp "$SOURCE_DIR/plugin.pb.cc" ./plugin.pb.sync.cc
cp "$SOURCE_DIR/timestamp.pb.h" ./timestamp.pb.sync.h
cp "$SOURCE_DIR/timestamp.pb.cc" ./timestamp.pb.sync.cc
cp "$SOURCE_DIR/duration.pb.h" ./duration.pb.sync.h
cp "$SOURCE_DIR/duration.pb.cc" ./duration.pb.sync.cc

amend_file() {
  local file="$1"

  # Replace all #include "<name>.pb.h" with #include "<name>.pb.sync.h"
  sed -i -E 's/#include "([^"]+)\.pb\.h"/#include "\1.pb.sync.h"/g' "$file"

  # fix formatting
  clang-format -i "$file"
}

amend_file ./descriptor.pb.sync.h
amend_file ./descriptor.pb.sync.cc
amend_file ./plugin.pb.sync.h
amend_file ./plugin.pb.sync.cc
amend_file ./timestamp.pb.sync.h
amend_file ./timestamp.pb.sync.cc
amend_file ./duration.pb.sync.h
amend_file ./duration.pb.sync.cc

echo "Descriptor proto files synchronized successfully."
