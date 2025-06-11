/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_vector_types.hh"

#include "DNA_scene_types.h"

#include "COM_context.hh"
#include "COM_result.hh"

namespace blender::compositor {

/* Blur the input using a horizontal and a vertical separable blur passes given a certain radius
 * and filter type using SymmetricSeparableBlurWeights. The result is written to the given output,
 * which will be allocated internally and is thus expected not to be previously allocated. */
void symmetric_separable_blur(Context &context,
                              const Result &input,
                              Result &output,
                              const float2 &radius,
                              const int filter_type = R_FILTER_GAUSS);

}  // namespace blender::compositor
