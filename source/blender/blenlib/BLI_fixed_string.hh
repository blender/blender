/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstddef>

namespace blender {

/**
 * This can represent a string at a compile time in a way that can be used as template parameter.
 *
 * While std::string can be used at compile time, it is not a "structural type" and therefore
 * cannot be used as template parameter.
 */
template<size_t N> struct FixedString {
  char data[N];

  constexpr FixedString(const char (&str)[N])
  {
    for (size_t i = 0; i < N; i++) {
      data[i] = str[i];
    }
  }
};

}  // namespace blender
