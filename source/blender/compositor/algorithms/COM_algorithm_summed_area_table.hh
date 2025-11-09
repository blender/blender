/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "COM_context.hh"
#include "COM_result.hh"

namespace blender::compositor {

/* Possible operations to apply on pixels before computing the summed area table. The Square
 * operation, for instance, can be useful to compute image variance from sum of squares. */
enum class SummedAreaTableOperation : uint8_t {
  Identity,
  Square,
};

/* Computes a summed area table from the given input and write the table to the given output. A
 * summed are table is an image where each pixel contains the sum of all pixels in the areas down
 * and to its left toward the zero index, including the pixel itself. This table is particularly
 * useful to accelerate filters that requires averaging large rectangular areas of the input, like
 * a box filter. */
void summed_area_table(Context &context,
                       Result &input,
                       Result &output,
                       SummedAreaTableOperation operation = SummedAreaTableOperation::Identity);

/* Computes the sum of the rectangular region defined by the given lower and upper bounds from the
 * given summed area table. It is assumed that the given upper bound is larger than the given lower
 * bound, otherwise, undefined behavior is invoked. Looking at the diagram below, in order to
 * compute the sum of area X, we sample the table at each of the corners of the area X, to get:
 *
 *   Upper Right -> A + B + C + X      (1)
 *   Upper Left -> A + B               (2)
 *   Lower Right -> B + C              (3)
 *   Lower Left -> B                   (4)
 *
 * We start from (1) and subtract (2) and (3) to get rid of A and C to get:
 *
 *  (A + B + C + X) - (A + B) - (B + C) = (X - B)
 *
 * To get rid of B, we add (4) to get:
 *
 *  (X - B) + B = X
 *
 *         ^
 *         |
 *         +-------+-----+
 *         |       |     |
 *         |   A   |  X  |
 *         |       |     |
 *         +-------+-----+
 *         |       |     |
 *         |   B   |  C  |
 *         |       |     |
 *         o-------+-----+------>
 *
 * The aforementioned equation eliminates the edges between regions X, C, and A since they get
 * subtracted with C and A. To avoid this, we subtract 1 from the lower bound and fallback to zero
 * for out of bound sampling. */
inline float4 summed_area_table_sum(const Result &table,
                                    const int2 &lower_bound,
                                    const int2 &upper_bound)
{
  int2 corrected_lower_bound = lower_bound - int2(1);
  int2 corrected_upper_bound = math::min(table.domain().size - int2(1), upper_bound);
  float4 addend = float4(table.load_pixel_zero<Color>(corrected_upper_bound)) +
                  float4(table.load_pixel_zero<Color>(corrected_lower_bound));
  float4 subtrahend = float4(table.load_pixel_zero<Color>(
                          int2(corrected_lower_bound.x, corrected_upper_bound.y))) +
                      float4(table.load_pixel_zero<Color>(
                          int2(corrected_upper_bound.x, corrected_lower_bound.y)));
  return addend - subtrahend;
}

}  // namespace blender::compositor
