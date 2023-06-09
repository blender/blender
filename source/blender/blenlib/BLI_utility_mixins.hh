/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#pragma once

namespace blender {

/**
 * A type that inherits from NonCopyable cannot be copied anymore.
 */
class NonCopyable {
 public:
  /* Disable copy construction and assignment. */
  NonCopyable(const NonCopyable &other) = delete;
  NonCopyable &operator=(const NonCopyable &other) = delete;

  /* Explicitly enable default construction, move construction and move assignment. */
  NonCopyable() = default;
  NonCopyable(NonCopyable &&other) = default;
  NonCopyable &operator=(NonCopyable &&other) = default;
};

/**
 * A type that inherits from NonMovable cannot be moved anymore.
 */
class NonMovable {
 public:
  /* Disable move construction and assignment. */
  NonMovable(NonMovable &&other) = delete;
  NonMovable &operator=(NonMovable &&other) = delete;

  /* Explicitly enable default construction, copy construction and copy assignment. */
  NonMovable() = default;
  NonMovable(const NonMovable &other) = default;
  NonMovable &operator=(const NonMovable &other) = default;
};

}  // namespace blender
