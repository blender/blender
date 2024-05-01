/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_vector_types.hh"

#include "COM_context.hh"
#include "COM_result.hh"

namespace blender::realtime_compositor {

/* Blur the input using a recursive Gaussian blur algorithm given a certain radius. This differs
 * from the standard symmetric separable blur algorithm in that it is orders of magnitude faster
 * for very high radius value, the downside is that it consumes more memory and is only an
 * approximation that might suffer from fringing and artifacts, though those are typically
 * unnoticeable. Neumann boundary is assumed.
 *
 * The output is written to the given output result, which will be allocated internally and is thus
 * expected not to be previously allocated. */
void recursive_gaussian_blur(Context &context, Result &input, Result &output, float2 radius);

}  // namespace blender::realtime_compositor
