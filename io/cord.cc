#include "io/cord.h"

#include <cstddef>
#include <utility>

#include "absl/log/check.h"
#include "io/buffer.h"

namespace tsdb2 {
namespace io {

size_t Cord::size() const {
  if (pieces_.empty()) {
    return 0;
  } else {
    auto const& last_piece = pieces_.back();
    return last_piece.offset + last_piece.buffer.size();
  }
}

void Cord::Append(Buffer buffer) {
  if (!buffer.empty()) {
    pieces_.emplace_back(size(), std::move(buffer));
  }
}

void Cord::Append(Cord other) {
  size_t offset = size();
  for (auto& [unused_offset, buffer] : other.pieces_) {
    auto const piece_size = buffer.size();
    pieces_.emplace_back(offset, std::move(buffer));
    offset += piece_size;
  }
}

Buffer Cord::Flatten() && {
  if (pieces_.size() < 1) {
    return Buffer();
  } else if (pieces_.size() < 2) {
    return std::move(pieces_.front().buffer);
  } else {
    auto const& last = pieces_.back();
    auto const size = last.offset + last.buffer.size();
    Buffer buffer{size};
    for (auto const& [unused_offset, piece] : pieces_) {
      buffer.Append(piece);
    }
    return buffer;
  }
}

Cord::Piece& Cord::GetPieceForIndex(size_t const index) {
  CHECK(!pieces_.empty());
  {
    auto const& last_piece = pieces_.back();
    CHECK_LT(index, last_piece.offset + last_piece.buffer.size());
  }
  size_t i = 0;
  size_t j = pieces_.size() - 1;
  while (i < j) {
    size_t k = j - ((j - i) >> 1);
    auto& piece = pieces_[k];
    if (index < piece.offset) {
      j = k - 1;
    } else if (index > piece.offset) {
      i = k;
    } else {
      return piece;
    }
  }
  return pieces_[i];
}

}  // namespace io
}  // namespace tsdb2
