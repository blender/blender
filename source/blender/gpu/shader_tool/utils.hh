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

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace blender::gpu::shader::parser {

using report_callback = std::function<void(
    int error_line, int error_char, std::string error_line_string, const char *error_str)>;

/* Poor man's IndexRange. */
struct IndexRange {
  int64_t start;
  int64_t size;

  IndexRange(size_t start, size_t size) : start(start), size(size) {}

  bool overlaps(IndexRange other) const
  {
    return ((start < other.start) && (other.start < (start + size))) ||
           ((other.start < start) && (start < (other.start + other.size)));
  }

  int64_t last()
  {
    return start + size - 1;
  }
};

/* Poor man's OffsetIndices. */
struct OffsetIndices {
  std::vector<size_t> offsets;

  IndexRange operator[](const int64_t index) const
  {
    return {offsets[index], offsets[index + 1] - offsets[index]};
  }

  void clear()
  {
    offsets.clear();
  };
};

struct TimeIt {
  using Duration = std::chrono::microseconds;

  Duration &time;
  std::chrono::high_resolution_clock::time_point start;

  TimeIt(Duration &time) : time(time)
  {
    start = std::chrono::high_resolution_clock::now();
  }
  ~TimeIt()
  {
    auto end = std::chrono::high_resolution_clock::now();
    time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  }
};

static inline size_t line_number(const std::string &prefix_string)
{
  std::string directive = "#line ";
  /* String to count the number of line. */
  std::string sub_str = prefix_string;
  size_t nearest_line_directive = sub_str.rfind(directive);
  size_t line_count = 1;
  if (nearest_line_directive != std::string::npos) {
    sub_str = sub_str.substr(nearest_line_directive + directive.size());
    line_count = std::stoll(sub_str) - 1;
  }
  return line_count + std::count(sub_str.begin(), sub_str.end(), '\n');
}

}  // namespace blender::gpu::shader::parser
