/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

namespace blender::math {

enum class FilterKernel {
  Box,
  Tent,
  Quad,
  Cubic,
  Catrom,
  Gauss,
  Mitch, /* Mitchell & Netravali's two-param cubic */
};

/**
 * \param x: ranges from -1 to 1.
 */
float filter_kernel_value(FilterKernel kernel, float x);

}  // namespace blender::math
