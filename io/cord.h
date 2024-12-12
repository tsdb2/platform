#ifndef __TSDB2_IO_CORD_H__
#define __TSDB2_IO_CORD_H__

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "absl/container/inlined_vector.h"
#include "io/buffer.h"

namespace tsdb2 {
namespace io {

class Cord {
 public:
  template <typename... Args,
            std::enable_if_t<std::conjunction_v<std::is_same<Args, Buffer>...>, bool> = true>
  explicit Cord(Args... pieces) {
    pieces_.reserve(sizeof...(pieces));
    size_t offset = 0;
    size_t size;
    ((size = pieces.size(), pieces_.emplace_back(offset, std::move(pieces)), offset += size), ...);
  }

  ~Cord() = default;

  Cord(Cord &&) noexcept = default;
  Cord &operator=(Cord &&) noexcept = default;

  void swap(Cord &other) noexcept { pieces_.swap(other.pieces_); }
  friend void swap(Cord &lhs, Cord &rhs) noexcept { lhs.swap(rhs); }

  size_t size() const;

  uint8_t &at(size_t const index) {
    auto &[offset, buffer] = GetPieceForIndex(index);
    return buffer.at<uint8_t>(index - offset);
  }

  uint8_t at(size_t const index) const {
    auto const &[offset, buffer] = GetPieceForIndex(index);
    return buffer.at<uint8_t>(index - offset);
  }

  uint8_t &operator[](size_t const index) { return at(index); }
  uint8_t operator[](size_t const index) const { return at(index); }

  void Append(Buffer buffer);

  void Append(Cord other);

  Buffer Flatten() &&;

 private:
  struct Piece {
    explicit Piece(size_t const offset, Buffer buffer)
        : offset(offset), buffer(std::move(buffer)) {}

    ~Piece() = default;

    Piece(Piece const &) = delete;
    Piece &operator=(Piece const &) = delete;
    Piece(Piece &&) noexcept = default;
    Piece &operator=(Piece &&) noexcept = default;

    void swap(Piece &other) noexcept {
      using std::swap;  // ensure ADL
      swap(offset, other.offset);
      swap(buffer, other.buffer);
    }

    friend void swap(Piece &lhs, Piece &rhs) noexcept { lhs.swap(rhs); }

    size_t offset;
    Buffer buffer;
  };

  Cord(Cord const &) = delete;
  Cord &operator=(Cord const &) = delete;

  Piece &GetPieceForIndex(size_t index);

  Piece const &GetPieceForIndex(size_t index) const {
    return const_cast<Cord *>(this)->GetPieceForIndex(index);
  }

  absl::InlinedVector<Piece, 1> pieces_;
};

}  // namespace io
}  // namespace tsdb2

#endif  // __TSDB2_IO_CORD_H__
