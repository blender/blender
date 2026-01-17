/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tools
 *
 * Lite duplicates of blender utility types and other utility functions.
 * They are duplicated to avoid pulling half of blender as a dependency.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace blender::gpu::shader::parser {

using report_callback = std::function<void(
    int error_line, int error_char, std::string error_line_string, const char *error_str)>;

/** Poor man's IndexRange. */
struct IndexRange {
  int64_t start;
  int64_t size;

  IndexRange(int64_t start, int64_t size) : start(start), size(size) {}

  bool overlaps(IndexRange other) const
  {
    if (start == other.start && size == other.size) {
      return true;
    }
    return ((start < other.start) && (other.start < (start + size))) ||
           ((other.start < start) && (start < (other.start + other.size)));
  }

  int64_t last()
  {
    return start + size - 1;
  }
};

/** Poor man's MutableSpan. */
template<typename T> struct MutableSpan {
  T *data_;
  uint64_t size_;

  T &operator[](const int64_t index)
  {
    return data_[index];
  }
  const T &operator[](const int64_t index) const
  {
    return data_[index];
  }

  T *data()
  {
    return data_;
  }

  uint64_t size() const
  {
    return size_;
  }

  T back() const
  {
    return (*this)[size_ - 1];
  }

  T *begin()
  {
    return data_;
  }
  T *end()
  {
    return data_ + size_;
  }

  const T *begin() const
  {
    return data_;
  }
  const T *end() const
  {
    return data_ + size_;
  }

  /**
   * Set span size to a smaller size, this invokes undefined behavior when n is negative or bigger
   * than the current span.
   */
  void shrink(int64_t new_size)
  {
    size_ = new_size;
  }
};

/** Poor man's OffsetIndices. */
struct OffsetIndices {
  MutableSpan<uint32_t> offsets;

  IndexRange operator[](const int64_t index) const
  {
    return {int64_t(offsets[index]), int64_t(offsets[index + 1] - offsets[index])};
  }

  uint32_t *data()
  {
    return offsets.data();
  }

  uint32_t size() const
  {
    return offsets.size() - 1;
  }
};

/** Return the line number this token is found at. Take into account the #line directives. */
size_t line_number(const std::string_view &str, size_t pos);
/** Return the offset to the start of the line. */
size_t char_number(const std::string_view &str, size_t pos);
/** Returns a string of the line containing the character at the given position. */
std::string line_str(const std::string_view &str, size_t pos);

}  // namespace blender::gpu::shader::parser
