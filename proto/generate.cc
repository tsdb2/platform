#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "common/utilities.h"
#include "proto/descriptor.h"
#include "proto/generator.h"
#include "proto/object.h"

ABSL_FLAG(std::optional<std::string>, root_path, "",
          "The root directory all the file descriptor names are relative to. Defaults to the "
          "current working directory if unspecified.");

ABSL_FLAG(std::vector<std::string>, file_descriptor_sets, {},
          "One or more comma-separated file paths containing serialized FileDescriptorSet "
          "protobufs. These paths must be relative to the current working directory; the root_path "
          "flag is ignored here.");

namespace {

using ::google::protobuf::FileDescriptorProto;
using ::google::protobuf::FileDescriptorSet;
using ::google::protobuf::kFileDescriptorProtoNameField;
using ::google::protobuf::kFileDescriptorSetFileField;
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

std::string GetFilePath(std::string_view const file_name) {
  if (absl::StartsWith(file_name, "/")) {
    return std::string(file_name);
  }
  auto const maybe_root_path = absl::GetFlag(FLAGS_root_path);
  if (maybe_root_path.has_value()) {
    auto const& root_path = maybe_root_path.value();
    if (absl::EndsWith(root_path, "/")) {
      return absl::StrCat(root_path, file_name);
    } else {
      return absl::StrCat(root_path, "/", file_name);
    }
  } else {
    return std::string(file_name);
  }
}

absl::Status GenerateFilePair(FileDescriptorProto const& descriptor) {
  DEFINE_CONST_OR_RETURN(name, RequireField<kFileDescriptorProtoNameField>(descriptor));
  LOG(INFO) << "generating header/source pair for " << name;
  DEFINE_VAR_OR_RETURN(generator, Generator::Create(descriptor));
  {
    DEFINE_CONST_OR_RETURN(header, generator.GenerateHeaderFileContent());
    auto const file_path = GetFilePath(MakeHeaderFileName(*name));
    LOG(INFO) << "writing " << file_path;
    DEFINE_VAR_OR_RETURN(header_file, File::Create(file_path));
    RETURN_IF_ERROR(header_file.Write(header));
  }
  {
    DEFINE_CONST_OR_RETURN(source, generator.GenerateSourceFileContent());
    auto const file_path = GetFilePath(MakeSourceFileName(*name));
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
  for (auto const& descriptor : descriptor_set.get<kFileDescriptorSetFileField>()) {
    auto const status = GenerateFilePair(descriptor);
    LOG_IF(ERROR, !status.ok()) << "" << status;
  }
  return absl::OkStatus();
}

absl::Status Run() {
  LOG(INFO) << "current working directory: " << ::get_current_dir_name();
  LOG(INFO) << "root path: " << absl::GetFlag(FLAGS_root_path).value_or("<none>");
  for (auto const& file_path : absl::GetFlag(FLAGS_file_descriptor_sets)) {
    auto const status = ProcessFileDescriptorSet(file_path);
    LOG_IF(ERROR, !status.ok()) << "" << status;
  }
  LOG(INFO) << "done";
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
