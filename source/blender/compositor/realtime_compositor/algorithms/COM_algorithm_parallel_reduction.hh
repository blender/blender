/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_vec_types.hh"

#include "GPU_texture.h"

#include "COM_context.hh"

namespace blender::realtime_compositor {

/* --------------------------------------------------------------------
 * Sum Reductions.
 */

/* Computes the sum of the red channel of all pixels in the given texture. */
float sum_red(Context &context, GPUTexture *texture);

/* Computes the sum of the green channel of all pixels in the given texture. */
float sum_green(Context &context, GPUTexture *texture);

/* Computes the sum of the blue channel of all pixels in the given texture. */
float sum_blue(Context &context, GPUTexture *texture);

/* Computes the sum of the luminance of all pixels in the given texture, using the given luminance
 * coefficients to compute the luminance. */
float sum_luminance(Context &context, GPUTexture *texture, float3 luminance_coefficients);

/* --------------------------------------------------------------------
 * Sum Of Squared Difference Reductions.
 */

/* Computes the sum of the squared difference between the red channel of all pixels in the given
 * texture and the given subtrahend. This can be used to compute the standard deviation if the
 * given subtrahend is the mean. */
float sum_red_squared_difference(Context &context, GPUTexture *texture, float subtrahend);

/* Computes the sum of the squared difference between the green channel of all pixels in the given
 * texture and the given subtrahend. This can be used to compute the standard deviation if the
 * given subtrahend is the mean. */
float sum_green_squared_difference(Context &context, GPUTexture *texture, float subtrahend);

/* Computes the sum of the squared difference between the blue channel of all pixels in the given
 * texture and the given subtrahend. This can be used to compute the standard deviation if the
 * given subtrahend is the mean. */
float sum_blue_squared_difference(Context &context, GPUTexture *texture, float subtrahend);

/* Computes the sum of the squared difference between the luminance of all pixels in the given
 * texture and the given subtrahend, using the given luminance coefficients to compute the
 * luminance. This can be used to compute the standard deviation if the given subtrahend is the
 * mean. */
float sum_luminance_squared_difference(Context &context,
                                       GPUTexture *texture,
                                       float3 luminance_coefficients,
                                       float subtrahend);

}  // namespace blender::realtime_compositor
