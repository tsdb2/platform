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
#include "proto/descriptor.h"
#include "proto/generator.h"
#include "proto/object.h"

ABSL_FLAG(
    std::vector<std::string>, file_descriptor_sets, {},
    "One or more comma-separated file paths containing serialized FileDescriptorSet protobufs.");

namespace {

using ::google::protobuf::FileDescriptorProto;
using ::google::protobuf::FileDescriptorSet;
using ::google::protobuf::kFileDescriptorProtoNameField;
using ::google::protobuf::kFileDescriptorSetFileField;
using ::tsdb2::proto::RequireField;
using ::tsdb2::proto::generator::GenerateHeaderFileContent;
using ::tsdb2::proto::generator::GenerateSourceFileContent;
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
  DEFINE_CONST_OR_RETURN(name, RequireField<kFileDescriptorProtoNameField>(descriptor));
  {
    DEFINE_CONST_OR_RETURN(header, GenerateHeaderFileContent(descriptor));
    DEFINE_VAR_OR_RETURN(header_file, File::Create(MakeHeaderFileName(*name)));
    RETURN_IF_ERROR(header_file.Write(header));
  }
  {
    DEFINE_CONST_OR_RETURN(source, GenerateSourceFileContent(descriptor));
    DEFINE_VAR_OR_RETURN(source_file, File::Create(MakeSourceFileName(*name)));
    RETURN_IF_ERROR(source_file.Write(source));
  }
  return absl::OkStatus();
}

absl::StatusOr<FileDescriptorSet> ReadFileDescriptorSet(std::string_view const file_path) {
  DEFINE_VAR_OR_RETURN(file, File::Open(std::string(file_path)));
  return file.ParseFileDescriptorSet();
}

absl::Status ProcessFileDescriptorSet(std::string_view const proto_file_path) {
  DEFINE_CONST_OR_RETURN(descriptor_set, ReadFileDescriptorSet(proto_file_path));
  for (auto const& descriptor : descriptor_set.get<kFileDescriptorSetFileField>()) {
    auto const status = GenerateFilePair(descriptor);
    LOG_IF(ERROR, !status.ok()) << status;
  }
  return absl::OkStatus();
}

absl::Status Run() {
  for (auto const& file_path : absl::GetFlag(FLAGS_file_descriptor_sets)) {
    auto const status = ProcessFileDescriptorSet(file_path);
    LOG_IF(ERROR, !status.ok()) << status;
  }
  return absl::OkStatus();
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
