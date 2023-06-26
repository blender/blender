/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_vector_types.hh"

#include "DNA_scene_types.h"

#include "COM_context.hh"
#include "COM_result.hh"

namespace blender::realtime_compositor {

/* Blur the input using a horizontal and a vertical separable blur passes given a certain radius
 * and filter type using SymmetricSeparableBlurWeights. The output is written to the given output
 * result, which will be allocated internally and is thus expected not to be previously allocated.
 * If extend_bounds is true, the output will have an extra radius amount of pixels on the boundary
 * of the image, where blurring can take place assuming a fully transparent out of bound values. If
 * gamma_correct is true, the input will be gamma corrected before blurring and then uncorrected
 * after blurring, using a gamma coefficient of 2. */
void symmetric_separable_blur(Context &context,
                              Result &input,
                              Result &output,
                              float2 radius,
                              int filter_type = R_FILTER_GAUSS,
                              bool extend_bounds = false,
                              bool gamma_correct = false);

}  // namespace blender::realtime_compositor
