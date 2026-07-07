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

namespace blender::gpu::shader::parser {

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

}  // namespace blender::gpu::shader::parser
