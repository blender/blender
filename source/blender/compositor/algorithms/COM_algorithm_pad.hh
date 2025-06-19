/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_vector_types.hh"

#include "COM_context.hh"
#include "COM_result.hh"

namespace blender::compositor {

/* Possible padding method to use. */
enum class PaddingMethod : uint8_t {
  /* Pads with zeros. */
  Zero,
  /* Pads by extending the edge. */
  Extend,
};

/* Pads the given input in both directions with the given size number of pixels. The output will be
 * allocated internally and is thus expected not to be previously allocated. */
void pad(Context &context,
         const Result &input,
         Result &output,
         const int2 size,
         const PaddingMethod padding_method);

}  // namespace blender::compositor
