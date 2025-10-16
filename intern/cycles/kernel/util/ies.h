/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"

CCL_NAMESPACE_BEGIN

/* IES Light */

ccl_device_inline float interpolate_ies_vertical(KernelGlobals kg,
                                                 const int ofs,
                                                 const bool wrap_vlow,
                                                 const bool wrap_vhigh,
                                                 const int v,
                                                 const int v_num,
                                                 const float v_frac,
                                                 const int h)
{
  /* Since lookups are performed in spherical coordinates, clamping the coordinates at the low end
   * of v (corresponding to the north pole) would result in artifacts. The proper way of dealing
   * with this would be to lookup the corresponding value on the other side of the pole, but since
   * the horizontal coordinates might be nonuniform, this would require yet another interpolation.
   * Therefore, the assumption is made that the light is going to be symmetrical, which means that
   * we can just take the corresponding value at the current horizontal coordinate. */

#define IES_LOOKUP(v) kernel_data_fetch(ies, ofs + h * v_num + (v))

  /* Look up the inner two points directly. */
  const float c = IES_LOOKUP(v + 1);
  const float b = IES_LOOKUP(v);

  /* Look up first point, or fall back to second point if not available. */
  float a = b;
  if (v > 0) {
    a = IES_LOOKUP(v - 1);
  }
  else if (wrap_vlow) {
    a = IES_LOOKUP(1);
  }

  /* Look up last point, or fall back to third point if not available. */
  float d = c;
  if (v + 2 < v_num) {
    d = IES_LOOKUP(v + 2);
  }
  else if (wrap_vhigh) {
    d = IES_LOOKUP(v_num - 2);
  }

#undef IES_LOOKUP

  return cubic_interp(a, b, c, d, v_frac);
}

ccl_device_inline float kernel_ies_interp(KernelGlobals kg,
                                          const int slot,
                                          const float h_angle,
                                          const float v_angle)
{
  /* Find offset of the IES data in the table. */
  int ofs = __float_as_int(kernel_data_fetch(ies, slot));
  if (ofs == -1) {
    return 100.0f;
  }

  const int h_num = __float_as_int(kernel_data_fetch(ies, ofs++));
  const int v_num = __float_as_int(kernel_data_fetch(ies, ofs++));

#define IES_LOOKUP_ANGLE_H(h) kernel_data_fetch(ies, ofs + (h))
#define IES_LOOKUP_ANGLE_V(v) kernel_data_fetch(ies, ofs + h_num + (v))

  /* Check whether the angle is within the bounds of the IES texture. */
  const float v_low = IES_LOOKUP_ANGLE_V(0);
  const float v_high = IES_LOOKUP_ANGLE_V(v_num - 1);
  const float h_low = IES_LOOKUP_ANGLE_H(0);
  const float h_high = IES_LOOKUP_ANGLE_H(h_num - 1);
  if (v_angle < v_low || v_angle >= v_high) {
    return 0.0f;
  }
  if (h_angle < h_low || h_angle >= h_high) {
    return 0.0f;
  }

  /* If the texture covers the full 360° range horizontally, wrap around the lookup
   * to get proper cubic interpolation. Otherwise, just set the out-of-range values to zero.
   * Similar logic for V, but there we check the lower and upper wrap separately. */
  const bool wrap_h = (h_low < 1e-7f && h_high > M_2PI_F - 1e-7f);
  const bool wrap_vlow = (v_low < 1e-7f);
  const bool wrap_vhigh = (v_high > M_PI_F - 1e-7f);

  /* Lookup the angles to find the table position. */
  int h_i;
  int v_i;
  /* TODO(lukas): Consider using bisection.
   * Probably not worth it for the vast majority of IES files. */
  for (h_i = 0; IES_LOOKUP_ANGLE_H(h_i + 1) < h_angle; h_i++) {
    ;
  }
  for (v_i = 0; IES_LOOKUP_ANGLE_V(v_i + 1) < v_angle; v_i++) {
    ;
  }

  const float h_frac = inverse_lerp(IES_LOOKUP_ANGLE_H(h_i), IES_LOOKUP_ANGLE_H(h_i + 1), h_angle);
  const float v_frac = inverse_lerp(IES_LOOKUP_ANGLE_V(v_i), IES_LOOKUP_ANGLE_V(v_i + 1), v_angle);

#undef IES_LOOKUP_ANGLE_H
#undef IES_LOOKUP_ANGLE_V

  /* Skip forward to the actual intensity data. */
  ofs += h_num + v_num;

  /* Interpolate the inner two points directly. */
  const float b = interpolate_ies_vertical(
      kg, ofs, wrap_vlow, wrap_vhigh, v_i, v_num, v_frac, h_i);
  const float c = interpolate_ies_vertical(
      kg, ofs, wrap_vlow, wrap_vhigh, v_i, v_num, v_frac, h_i + 1);

  /* Interpolate first point, or fall back to second point if not available. */
  float a = b;
  if (h_i > 0) {
    a = interpolate_ies_vertical(kg, ofs, wrap_vlow, wrap_vhigh, v_i, v_num, v_frac, h_i - 1);
  }
  else if (wrap_h) {
    /* The last entry (360°) equals the first one, so we need to wrap around to the one before. */
    a = interpolate_ies_vertical(kg, ofs, wrap_vlow, wrap_vhigh, v_i, v_num, v_frac, h_num - 2);
  }

  /* Interpolate last point, or fall back to second point if not available. */
  float d = b;
  if (h_i + 2 < h_num) {
    d = interpolate_ies_vertical(kg, ofs, wrap_vlow, wrap_vhigh, v_i, v_num, v_frac, h_i + 2);
  }
  else if (wrap_h) {
    /* Same logic here, wrap around to the second element if necessary. */
    d = interpolate_ies_vertical(kg, ofs, wrap_vlow, wrap_vhigh, v_i, v_num, v_frac, 1);
  }

  /* Cubic interpolation can result in negative values, so get rid of them. */
  return max(cubic_interp(a, b, c, d, h_frac), 0.0f);
}

CCL_NAMESPACE_END
