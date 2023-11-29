/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_scene_types.h"

#include "COM_context.hh"
#include "COM_result.hh"

namespace blender::realtime_compositor {

/* Blur the input using a horizontal and a vertical separable blur passes given the filter type
 * using SymmetricSeparableBlurWeights, where the number of weights is equal to weights_resolution.
 * Since the radius can be variable, the number of weights can be less than or more than the number
 * of pixels actually getting accumulated during blurring, so the weights are interpolated in the
 * shader as needed, the resolution is typically set to the maximum possible radius if known. The
 * radius of the blur can be variable and is defined using the given radius float image. The output
 * is written to the given output result, which will be allocated internally and is thus expected
 * not to be previously allocated.
 *
 * Technically, variable size blur can't be computed separably, however, assuming a sufficiently
 * smooth radius field, the results can be visually pleasing, so this can be used a more performant
 * variable size blur if the quality is satisfactory. */
void symmetric_separable_blur_variable_size(Context &context,
                                            Result &input,
                                            Result &output,
                                            Result &radius,
                                            int filter_type = R_FILTER_GAUSS,
                                            int weights_resolution = 128);

}  // namespace blender::realtime_compositor
