/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_vector_types.hh"

#include "COM_context.hh"
#include "COM_result.hh"

namespace blender::compositor {

/* --------------------------------------------------------------------
 * Sum Reductions.
 */

/* Computes the sum of all pixels in the given input. */
float4 sum_color(Context &context, const Result &input);

/* Computes the sum of the logarithm of the luminance of all pixels in the given color input, using
 * the given luminance coefficients to compute the luminance. */
float sum_log_luminance(Context &context,
                        const Result &input,
                        const float3 &luminance_coefficients);

/* --------------------------------------------------------------------
 * Sum Of Squared Difference Reductions.
 */

/* Computes the sum of the squared difference between the all pixels in the given input and the
 * given subtrahend. This can be used to compute the standard deviation if the given subtrahend is
 * the mean. */
float4 sum_squared_difference_color(Context &context,
                                    const Result &input,
                                    const float4 subtrahend);

/* --------------------------------------------------------------------
 * Minimum Reductions.
 */

/* Computes the minimum value of all pixels in the given input. */
float minimum_float(Context &context, const Result &input);

/* Computes the minimum luminance of all pixels in the given input, using the given luminance
 * coefficients to compute the luminance. */
float minimum_luminance(Context &context,
                        const Result &input,
                        const float3 &luminance_coefficients);

/* Computes the minimum float of all pixels in the given float input, limited to the given range.
 * Values outside of the given range are ignored. If non of the pixel values are in the range, the
 * upper bound of the range is returned. For instance, if the given range is [-10, 10] and the
 * image contains the values {-11, 2, 5}, the minimum will be 2, since -11 is outside of the range.
 * This is particularly useful for Z Depth normalization, since Z Depth can contain near infinite
 * values, so enforcing a lower bound is beneficial. */
float minimum_float_in_range(Context &context,
                             const Result &input,
                             const float lower_bound,
                             const float upper_bound);

/* --------------------------------------------------------------------
 * Maximum Reductions.
 */

/* Computes the maximum value of all pixels in the given input. */
float maximum_float(Context &context, const Result &input);
float2 maximum_float2(Context &context, const Result &input);

/* Computes the maximum luminance of all pixels in the given input, using the given luminance
 * coefficients to compute the luminance. */
float maximum_luminance(Context &context,
                        const Result &input,
                        const float3 &luminance_coefficients);

/* Computes the maximum float of all pixels in the given float input, limited to the given range.
 * Values outside of the given range are ignored. If non of the pixel values are in the range, the
 * lower bound of the range is returned. For instance, if the given range is [-10, 10] and the
 * image contains the values {2, 5, 11}, the maximum will be 5, since 11 is outside of the range.
 * This is particularly useful for Z Depth normalization, since Z Depth can contain near infinite
 * values, so enforcing an upper bound is beneficial. */
float maximum_float_in_range(Context &context,
                             const Result &input,
                             const float lower_bound,
                             const float upper_bound);

}  // namespace blender::compositor
