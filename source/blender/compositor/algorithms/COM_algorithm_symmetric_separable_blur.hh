/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_filter.hh"
#include "BLI_math_vector_types.hh"

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
                              const math::FilterKernel filter_type = math::FilterKernel::Gauss);

}  // namespace blender::compositor
