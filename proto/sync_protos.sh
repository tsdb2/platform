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

echo "Descriptor proto files synchronized successfully."
