#ifndef __TSDB2_PROTO_DEPENDENCIES_H__
#define __TSDB2_PROTO_DEPENDENCIES_H__

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"
#include "absl/types/span.h"

namespace tsdb2 {
namespace proto {
namespace internal {

// Keeps track of the dependencies among messages and allows inferring a definition order (for both
// global scope message and nested messages) that doesn't require forward declarations. Also allows
// erroring out when a cycle is detected.
//
// This class keeps track of a separate dependency graph for each scope. If a message `A` depends on
// a *nested* message `B.C`, the dependency won't affect the order of the messages inside `B`, it
// will only require that `B` itself is declared before `A`. More in general a dependency between
// two arbitrary paths `A0.A1.(...).An` and `B0.B1.(...).Bm` will only affect the dependency graph
// of the closest common ancestor. For example, in the following message definition:
//
//   message A0 {
//     message A1 {
//       message A2 {
//         message A3 {
//           // ...
//         }
//       }
//       message B2 {
//         message B3 {
//           optional A2.A3 a3 = 1;
//         }
//       }
//     }
//   }
//
// The only requirement is that `A2` is declared before `B2`. That is a requirement on the
// dependency graph of `A1`, the closest common ancestor. Despite having a field typed `A3`, `B3`
// doesn't generate any requirements on the dependency graphs of `A0`, `A2`, `A3`, `B2`, or `B3`.
//
// Because of the above, the `AddDependency` method will internally create a dependency between the
// respective children of the closest common ancestor rather than between the two paths specified.
//
// NOTE: in the protobuf compiler of the TSDB2 platform we cannot resolve the dependency issue by
// just forward-declaring all messages of each scope at the beginning of the scope. The very reason
// why we have a separate protobuf compiler is that we generally want to use `std::optional` rather
// than any kind of pointers for fields referring to another message, so nested messages won't be
// allocated in a separate memory block -- instead they'll be inlined inside the outer message. We
// do that because minimizing the number of heap allocations is one of the key micro-optimization
// goals of the TSDB2 platform, and platform-based programs are expected to be protobuf-heavy. That
// said, not all message dependencies can be handled by this class: there are legitimate use cases
// of circular dependencies among messages (think about the TSQL AST for example, or any other
// recursive data structure). In those cases we cannot inline messages inside each other infinitely,
// as doing so will simply cause a C++ compilation error. The circular dependency must necessarily
// be broken by making one the fields involved in it a pointer rather than an `std::optional`. The
// user is responsible for annotating such fields as "indirect", and we provide the `tsdb2.indirect`
// annotation for that purpose. For example, the following defines the protobuf version of a binary
// search tree:
//
//   import "proto/indirect.proto";
//
//   message TreeNode {
//     optional string label = 1;
//     optional TreeNode left [(tsdb2.indirect) = true] = 2;
//     optional TreeNode right [(tsdb2.indirect) = true] = 3;
//   }
//
// Indirect dependencies must NOT be registered in this class, so they won't contribute to the
// reported cycles. The caller is responsible for handling indirect dependencies by adding forward
// declarations at the top of the scope and rendering the fields as e.g. `std::unique_ptr`.
//
// Note that the cardinality of indirect fields MUST be optional, for two reasons:
//
//   1. requiring all the fields of a dependency loop means the involed message(s) have infinite
//      size;
//   2. pointer types are by definition nullable, so the behavior must be well defined when a null
//      pointer is serialized.
//
class DependencyManager {
 public:
  using Path = absl::InlinedVector<std::string, 2>;
  using PathView = absl::Span<std::string const>;
  using Cycle = std::vector<std::pair<Path, std::string>>;

  explicit DependencyManager() = default;
  ~DependencyManager() = default;

  DependencyManager(DependencyManager const&) = default;
  DependencyManager& operator=(DependencyManager const&) = default;

  DependencyManager(DependencyManager&&) noexcept = default;
  DependencyManager& operator=(DependencyManager&&) noexcept = default;

  // Registers a protobuf message without any dependencies. Dependencies may be added later with
  // `AddDependency`.
  //
  // `path` is a global path that identifies the added message. It's an array of path components
  // corresponding the fully qualified name of the message, for example `{"google", "protobuf",
  // "Timestamp"}` for `google.protobuf.Timestamp`. The `DependencyManager` treats path component
  // strings opaquely. The only aspect that matters is that each component defines a new nested
  // scoped with its own dependency graph.
  //
  // WARNING: the `path` must not be empty, this method check-fails if an empty path is specified.
  // Empty paths don't make sense because a message must at least have a name, and the
  // `DependencyManager` wouldn't know how to handle empty paths because `AddDependency` wouldn't be
  // able to find the closest common ancestor.
  void AddNode(PathView path);

  // Defines a dependency between `dependent` and `dependee`, which are global paths to messages as
  // explained in `AddNode`.
  //
  // REQUIRES: both the `dependee` and the `dependent` must have been previously added with
  // respective `AddNode` calls.
  //
  // `edge_name` is the name of the field creating the dependency. In other words, the message
  // identified by `dependent` has a field called like `edge_name` whose type is the message
  // identified by `dependee`.
  //
  // The `edge_name` is used only for reporting purposes when a cycle is found.
  //
  // WARNING: the `dependent` and `dependee` paths must not be empty, this method check-fails if an
  // empty path is specified. Empty paths don't make sense because a message must at least have a
  // name, and the `DependencyManager` wouldn't know how to handle empty paths because
  // `AddDependency` wouldn't be able to find the closest common ancestor.
  void AddDependency(PathView dependent, PathView dependee, std::string_view edge_name);

  // Searches for possible cycles in the dependency graph of the scope of the protobuf message
  // identified by `base_path`.
  //
  // Note that a `.proto` file will typically have many messages and many scopes, and this function
  // needs to be invoked separately for each one of them.
  //
  // The returned array is empty if there are no cycles. If one or more cycles are found, each one
  // is expressed as an array of (message, field) pairs. The last entry of the cycle is always the
  // same as the first, so it's omitted unless the cycle has a single node with a self-edge (in that
  // case the returned cycle has only one entry).
  std::vector<Cycle> FindCycles(PathView base_path) const;

  // REQUIRES: `FindCycles` MUST have returned an empty array.
  std::vector<std::string> MakeOrder(PathView base_path) const;

 private:
  class CycleFrame;

  class CycleFinder {
   public:
    explicit CycleFinder(DependencyManager const& parent, PathView const base_path,
                         std::vector<Cycle>* const cycles,
                         absl::flat_hash_set<std::string>* const visited)
        : parent_(parent), base_path_(base_path), cycles_(cycles), visited_(visited) {}

    ~CycleFinder() = default;

    void Run(std::string_view node, std::string_view edge_name) &&;

   private:
    friend class CycleFrame;

    CycleFinder(CycleFinder const&) = delete;
    CycleFinder& operator=(CycleFinder const&) = delete;
    CycleFinder(CycleFinder&&) = delete;
    CycleFinder& operator=(CycleFinder&&) = delete;

    bool PushFrame(std::string_view node, std::string_view edge);
    void PopFrame(std::string_view node);

    void RunInternal(std::string_view node, std::string_view edge_name);

    DependencyManager const& parent_;
    PathView const base_path_;
    std::vector<Cycle>* const cycles_;
    absl::flat_hash_set<std::string>* const visited_;
    absl::flat_hash_set<std::string_view> path_;
    Cycle stack_;
  };

  class CycleFrame {
   public:
    static std::optional<CycleFrame> Create(CycleFinder* parent, std::string_view node,
                                            std::string_view edge);

    explicit CycleFrame(CycleFinder* const parent, std::string_view const node)
        : parent_(parent), node_(node) {}

    ~CycleFrame();

   private:
    CycleFrame(CycleFrame const&) = delete;
    CycleFrame& operator=(CycleFrame const&) = delete;

    CycleFrame(CycleFrame&& other) noexcept;
    CycleFrame& operator=(CycleFrame&&) = delete;

    CycleFinder* parent_;
    std::string_view node_;
  };

  class OrderMaker {
   public:
    explicit OrderMaker(DependencyManager const& parent) : parent_(parent) {}

    std::vector<std::string> Run() &&;

   private:
    void RunInternal(std::string_view node);

    DependencyManager const& parent_;
    absl::flat_hash_set<std::string> visited_;
    std::vector<std::string> order_;
  };

  static Path MakePath(PathView base_path, std::string_view component);

  std::vector<std::string> GetRoots() const;

  absl::btree_map<std::string, absl::btree_map<std::string, std::string>> dependencies_;
  absl::flat_hash_map<std::string, DependencyManager> inner_dependencies_;
};

}  // namespace internal
}  // namespace proto
}  // namespace tsdb2

#endif  // __TSDB2_PROTO_DEPENDENCIES_H__
