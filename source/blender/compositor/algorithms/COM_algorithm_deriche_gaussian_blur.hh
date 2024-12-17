/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_vector_types.hh"

#include "COM_context.hh"
#include "COM_result.hh"

namespace blender::realtime_compositor {

/* Blur the input using a fourth order IIR filter approximating a Gaussian filter of the given
 * sigma computed using Deriche's design method. This is based on the following paper:
 *
 *   Deriche, Rachid. Recursively implementating the Gaussian and its derivatives. Diss. INRIA,
 *   1993.
 *
 * This differs from the standard symmetric separable blur algorithm in that it is faster for high
 * sigma values, the downside is that it consumes more memory and is only an approximation that
 * might suffer from fringing and artifacts, though those are typically unnoticeable. This filter
 * is numerically unstable and not accurate for sigma values larger than 32, in those cases, use
 * the Van Vliet filter instead. Further, for sigma values less than 3, use direct convolution
 * instead, since it is faster and more accurate. Neumann boundary is assumed.
 *
 * The output is written to the given output result, which will be allocated internally and is thus
 * expected not to be previously allocated. */
void deriche_gaussian_blur(Context &context, Result &input, Result &output, float2 sigma);

}  // namespace blender::realtime_compositor
