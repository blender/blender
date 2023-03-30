/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2015 Blender Foundation */

/** \file
 * \ingroup imbuf
 */

#ifndef __IMB_COLORMANAGEMENT_INLINE_C__
#define __IMB_COLORMANAGEMENT_INLINE_C__

#include "BLI_math_vector.h"
#include "IMB_colormanagement_intern.h"

void IMB_colormanagement_get_luminance_coefficients(float r_rgb[3])
{
  copy_v3_v3(r_rgb, imbuf_luma_coefficients);
}

float IMB_colormanagement_get_luminance(const float rgb[3])
{
  return dot_v3v3(imbuf_luma_coefficients, rgb);
}

uchar IMB_colormanagement_get_luminance_byte(const uchar rgb[3])
{
  float rgbf[3];
  float val;

  rgb_uchar_to_float(rgbf, rgb);
  val = dot_v3v3(imbuf_luma_coefficients, rgbf);

  return unit_float_to_uchar_clamp(val);
}

void IMB_colormanagement_xyz_to_scene_linear(float scene_linear[3], const float xyz[3])
{
  mul_v3_m3v3(scene_linear, imbuf_xyz_to_scene_linear, xyz);
}

void IMB_colormanagement_scene_linear_to_xyz(float xyz[3], const float scene_linear[3])
{
  mul_v3_m3v3(xyz, imbuf_scene_linear_to_xyz, scene_linear);
}

void IMB_colormanagement_rec709_to_scene_linear(float scene_linear[3], const float rec709[3])
{
  mul_v3_m3v3(scene_linear, imbuf_rec709_to_scene_linear, rec709);
}

void IMB_colormanagement_scene_linear_to_rec709(float rec709[3], const float scene_linear[3])
{
  mul_v3_m3v3(rec709, imbuf_scene_linear_to_rec709, scene_linear);
}

void IMB_colormanagement_scene_linear_to_srgb_v3(float srgb[3], const float scene_linear[3])
{
  mul_v3_m3v3(srgb, imbuf_scene_linear_to_rec709, scene_linear);
  linearrgb_to_srgb_v3_v3(srgb, srgb);
}

void IMB_colormanagement_srgb_to_scene_linear_v3(float scene_linear[3], const float srgb[3])
{
  srgb_to_linearrgb_v3_v3(scene_linear, srgb);
  mul_m3_v3(imbuf_rec709_to_scene_linear, scene_linear);
}

void IMB_colormanagement_aces_to_scene_linear(float scene_linear[3], const float aces[3])
{
  mul_v3_m3v3(scene_linear, imbuf_aces_to_scene_linear, aces);
}

void IMB_colormanagement_scene_linear_to_aces(float aces[3], const float scene_linear[3])
{
  mul_v3_m3v3(aces, imbuf_scene_linear_to_aces, scene_linear);
}

#endif /* __IMB_COLORMANAGEMENT_INLINE_H__ */
