/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#pragma once

#include <concepts>
#include <optional>

namespace blender::exec_mode {

/** Potentially use multiple threads to execute the function. */
struct Parallel {
  static constexpr bool is_parallel = true;
};

/** Execute the function in the current thread. */
struct Serial {
  static constexpr bool is_parallel = false;
};

/**
 * Potentially use multiple threads to execute the function, with a configurable grain size to
 * influence the parallel task size.
 */
struct ParallelGrainSize {
  static constexpr bool is_parallel = true;
  int grain_size = 1;
};

/**
 * Argument used to control whether a function should use parallel execution or not.
 */
template<typename T>
concept Tag = requires {
  {
    T::is_parallel
  } -> std::convertible_to<bool>;
};

/**
 * A version of #Tag that can be used in non-template functions.
 */
struct Mode {
  bool is_parallel;
  std::optional<int> grain_size;
  constexpr Mode(Parallel /*tag*/) : is_parallel(true), grain_size(std::nullopt) {}
  constexpr Mode(Serial /*tag*/) : is_parallel(false), grain_size(std::nullopt) {}
  constexpr Mode(ParallelGrainSize tag) : is_parallel(true), grain_size(tag.grain_size) {}
};

/** Main access points to control execution mode. */
constexpr Parallel parallel = Parallel();
constexpr Serial serial = Serial();
constexpr ParallelGrainSize grain_size(int grain_size)
{
  return ParallelGrainSize{grain_size};
}

}  // namespace blender::exec_mode
