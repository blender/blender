/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"

#include "COM_context.hh"
#include "COM_result.hh"

#include "COM_algorithm_deriche_gaussian_blur.hh"
#include "COM_algorithm_symmetric_separable_blur.hh"
#include "COM_algorithm_van_vliet_gaussian_blur.hh"

namespace blender::realtime_compositor {

/* Compute the Gaussian sigma from the radius, where the radius is in pixels. Blender's filter is
 * truncated at |x| > 3 * sigma as can be seen in the R_FILTER_GAUSS case of the RE_filter_value
 * function, so we divide by three to get the approximate sigma value. Further, ensure the radius
 * is at least 1 since recursive Gaussian implementations can't handle zero radii. */
static float2 compute_sigma_from_radius(float2 radius)
{
  return math::max(float2(1.0f), radius) / 3.0f;
}

/* Apply a recursive Gaussian blur algorithm on the input based on the general method outlined
 * in the following paper:
 *
 *  Hale, Dave. "Recursive gaussian filters." CWP-546 (2006).
 *
 * In particular, based on the table in Section 5 Conclusion, for very low radius blur, we use a
 * direct separable Gaussian convolution. For medium blur radius, we use the fourth order IIR
 * Deriche filter based on the following paper:
 *
 *   Deriche, Rachid. Recursively implementating the Gaussian and its derivatives. Diss. INRIA,
 *   1993.
 *
 * For high radius blur, we use the fourth order IIR Van Vliet filter based on the following paper:
 *
 *   Van Vliet, Lucas J., Ian T. Young, and Piet W. Verbeek. "Recursive Gaussian derivative
 *   filters." Proceedings. Fourteenth International Conference on Pattern Recognition (Cat. No.
 *   98EX170). Vol. 1. IEEE, 1998.
 *
 * That's because direct convolution is faster and more accurate for very low radius, while the
 * Deriche filter is more accurate for medium blur radius, while Van Vliet is more accurate for
 * high blur radius. The criteria suggested by the paper is a sigma value threshold of 3 and 32 for
 * the Deriche and Van Vliet filters respectively, which we apply on the larger of the two
 * dimensions. */
void recursive_gaussian_blur(Context &context, Result &input, Result &output, float2 radius)
{
  /* The radius is in pixel units, while both recursive implementations expect the sigma value of
   * the Gaussian function. */
  const float2 sigma = compute_sigma_from_radius(radius);

  if (math::reduce_max(sigma) < 3.0f) {
    symmetric_separable_blur(context, input, output, radius);
    return;
  }

  if (math::reduce_max(sigma) < 32.0f) {
    deriche_gaussian_blur(context, input, output, sigma);
    return;
  }

  van_vliet_gaussian_blur(context, input, output, sigma);
}

}  // namespace blender::realtime_compositor
