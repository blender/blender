/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

/* WORKAROUND: Workaround include order hell. */
#ifdef GLSL_CPP_STUBS
#elif defined(GPU_SHADER)
#  define static
#endif

class IndexRange {
 private:
  int start_;
  int size_;

 public:
  METAL_CONSTRUCTOR_2(IndexRange, int, start_, int, size_)

  static IndexRange from_begin_end(int begin, int end)
  {
    return IndexRange(begin, end - begin);
  }

  /**
   * Get the first element in the range.
   */
  int first() const
  {
    return this->start_;
  }

  /**
   * Get the first element in the range. The returned value is undefined when the range is empty.
   */
  int start() const
  {
    return this->start_;
  }

  /**
   * Get the nth last element in the range.
   */
  int last(int n = 0) const
  {
    return this->start_ + this->size_ - 1 - n;
  }

  /**
   * Get the amount of numbers in the range.
   */
  int size() const
  {
    return this->size_;
  }

  /**
   * Returns a new range, that contains a sub-interval of the current one.
   */
  IndexRange slice(int start, int size) const
  {
    int new_start = this->start_ + start;
    return IndexRange(new_start, size);
  }
  IndexRange slice(IndexRange range) const
  {
    return this->slice(range.start(), range.size());
  }

  /**
   * Move the range forward or backward within the larger array. The amount may be negative,
   * but its absolute value cannot be greater than the existing start of the range.
   */
  IndexRange shift(int n) const
  {
    return IndexRange(this->start_ + n, this->size_);
  }
};
