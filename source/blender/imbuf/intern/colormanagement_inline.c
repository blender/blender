/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2015 Blender Foundation. All rights reserved. */

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

void IMB_colormanagement_rec709_to_rgb(float rgb[3], const float rec709[3])
{
  mul_v3_m3v3(rgb, imbuf_rec709_to_xyz, rec709);
  mul_v3_m3v3(rgb, imbuf_xyz_to_rgb, rgb);
}

void IMB_colormanagement_rgb_to_rec709(float rec709[3], const float rgb[3])
{
  mul_v3_m3v3(rec709, imbuf_rgb_to_xyz, rgb);
  mul_v3_m3v3(rec709, imbuf_xyz_to_rec709, rec709);
}

void IMB_colormanagement_aces_to_rgb(float rgb[3], const float aces[3])
{
  mul_v3_m3v3(rgb, imbuf_aces_to_xyz, aces);
  mul_v3_m3v3(rgb, imbuf_xyz_to_rgb, rgb);
}

void IMB_colormanagement_rgb_to_aces(float aces[3], const float rgb[3])
{
  mul_v3_m3v3(aces, imbuf_rgb_to_xyz, rgb);
  mul_v3_m3v3(aces, imbuf_xyz_to_aces, aces);
}

#endif /* __IMB_COLORMANAGEMENT_INLINE_H__ */
