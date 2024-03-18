/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

namespace blender {

template<typename T> struct Bounds {
  T min;
  T max;
  Bounds() = default;
  Bounds(const T &value) : min(value), max(value) {}
  Bounds(const T &min, const T &max) : min(min), max(max) {}
};

}  // namespace blender
