/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2015 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup imbuf
 */

#ifndef __IMB_COLORMANAGEMENT_INLINE_C__
#define __IMB_COLORMANAGEMENT_INLINE_C__

#include "BLI_math_vector.h"
#include "IMB_colormanagement_intern.h"

float IMB_colormanagement_get_luminance(const float rgb[3])
{
  return dot_v3v3(imbuf_luma_coefficients, rgb);
}

unsigned char IMB_colormanagement_get_luminance_byte(const unsigned char rgb[3])
{
  float rgbf[3];
  float val;

  rgb_uchar_to_float(rgbf, rgb);
  val = dot_v3v3(imbuf_luma_coefficients, rgbf);

  return unit_float_to_uchar_clamp(val);
}

void IMB_colormanagement_xyz_to_rgb(float rgb[3], const float xyz[3])
{
  mul_v3_m3v3(rgb, imbuf_xyz_to_rgb, xyz);
}

void IMB_colormanagement_rgb_to_xyz(float xyz[3], const float rgb[3])
{
  mul_v3_m3v3(xyz, imbuf_rgb_to_xyz, rgb);
}

#endif /* __IMB_COLORMANAGEMENT_INLINE_H__ */
