#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "common/utilities.h"
#include "proto/descriptor.pb.sync.h"
#include "proto/generator.h"
#include "proto/proto.h"

ABSL_FLAG(std::string, proto_output_directory, "",
          "The directory where all the generated files are written. Defaults to the current "
          "working directory if unspecified.");

ABSL_FLAG(
    std::vector<std::string>, proto_file_descriptor_sets, {},
    "One or more comma-separated file paths containing serialized FileDescriptorSet protobufs.");

namespace {

using ::google::protobuf::FileDescriptorProto;
using ::google::protobuf::FileDescriptorSet;
using ::tsdb2::proto::Generator;
using ::tsdb2::proto::RequireField;
using ::tsdb2::proto::generator::MakeHeaderFileName;
using ::tsdb2::proto::generator::MakeSourceFileName;
using ::tsdb2::proto::generator::ReadFile;
using ::tsdb2::proto::generator::WriteFile;

class File {
 public:
  static absl::StatusOr<File> Open(std::string const& path) {
    return OpenInternal(path, /*mode=*/"r");
  }

  static absl::StatusOr<File> Create(std::string const& path) {
    return OpenInternal(path, /*mode=*/"w");
  }

  ~File() { MaybeClose(); }

  File(File const&) = delete;
  File& operator=(File const&) = delete;

  File(File&& other) noexcept : fp_(other.fp_) { other.fp_ = nullptr; }

  File& operator=(File&& other) noexcept {
    MaybeClose();
    fp_ = other.fp_;
    other.fp_ = nullptr;
    return *this;
  }

  absl::StatusOr<FileDescriptorSet> ParseFileDescriptorSet() const {
    DEFINE_CONST_OR_RETURN(data, ReadFile(fp_));
    return FileDescriptorSet::Decode(data);
  }

  absl::Status Write(std::string_view const content) {
    return WriteFile(fp_, absl::Span<uint8_t const>(
                              reinterpret_cast<uint8_t const*>(content.data()), content.size()));
  }

 private:
  static absl::StatusOr<File> OpenInternal(std::string const& path, std::string const& mode) {
    gsl::owner<FILE*> const fp = ::fopen(path.c_str(), mode.c_str());
    if (fp != nullptr) {
      return File(fp);
    } else {
      return absl::ErrnoToStatus(errno, "fopen");
    }
  }

  explicit File(gsl::owner<FILE*> const fp) : fp_(fp) {}

  void MaybeClose() {
    if (fp_ != nullptr) {
      LOG_IF(ERROR, ::fclose(fp_) < 0) << absl::ErrnoToStatus(errno, "fclose");
    }
  }

  gsl::owner<FILE*> fp_;
};

absl::Status GenerateFilePair(FileDescriptorProto const& descriptor) {
  REQUIRE_FIELD_OR_RETURN(name, descriptor, name);
  LOG(INFO) << "generating header/source pair for " << name;
  auto const& output_directory = absl::GetFlag(FLAGS_proto_output_directory);
  DEFINE_VAR_OR_RETURN(generator, Generator::Create(descriptor));
  {
    DEFINE_CONST_OR_RETURN(header, generator.GenerateHeaderFileContent());
    DEFINE_CONST_OR_RETURN(file_path, MakeHeaderFileName(output_directory, name));
    LOG(INFO) << "writing " << file_path;
    DEFINE_VAR_OR_RETURN(header_file, File::Create(file_path));
    RETURN_IF_ERROR(header_file.Write(header));
  }
  {
    DEFINE_CONST_OR_RETURN(source, generator.GenerateSourceFileContent(output_directory));
    DEFINE_CONST_OR_RETURN(file_path, MakeSourceFileName(output_directory, name));
    LOG(INFO) << "writing " << file_path;
    DEFINE_VAR_OR_RETURN(source_file, File::Create(file_path));
    RETURN_IF_ERROR(source_file.Write(source));
  }
  return absl::OkStatus();
}

absl::StatusOr<FileDescriptorSet> ReadFileDescriptorSet(std::string_view const file_path) {
  DEFINE_VAR_OR_RETURN(file, File::Open(std::string(file_path)));
  return file.ParseFileDescriptorSet();
}

absl::Status ProcessFileDescriptorSet(std::string_view const file_path) {
  LOG(INFO) << "processing " << file_path;
  DEFINE_CONST_OR_RETURN(descriptor_set, ReadFileDescriptorSet(file_path));
  absl::Status status = absl::OkStatus();
  for (auto const& descriptor : descriptor_set.file) {
    status.Update(GenerateFilePair(descriptor));
  }
  return status;
}

absl::Status Run() {
  LOG(INFO) << "current working directory: " << ::get_current_dir_name();
  LOG(INFO) << "output directory: " << absl::GetFlag(FLAGS_proto_output_directory);
  absl::Status status = absl::OkStatus();
  for (auto const& file_path : absl::GetFlag(FLAGS_proto_file_descriptor_sets)) {
    status.Update(ProcessFileDescriptorSet(file_path));
  }
  LOG(INFO) << "done";
  return status;
}

}  // namespace

int main(int const argc, char* argv[]) {
  absl::InitializeLog();
  absl::ParseCommandLine(argc, argv);
  auto const status = Run();
  if (!status.ok()) {
    std::string const message{status.message()};
    ::fprintf(stderr, "Error: %s\n", message.c_str());  // NOLINT
    return 1;
  }
  return 0;
}
