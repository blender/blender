/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#pragma once

#include "BLI_colorspace.hh"
#include "BLI_math_color.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "IMB_colormanagement_intern.hh"

namespace blender {

void IMB_colormanagement_get_luminance_coefficients(float r_rgb[3])
{
  copy_v3_v3(r_rgb, colorspace::luma_coefficients);
}

float IMB_colormanagement_get_luminance(const float rgb[3])
{
  return dot_v3v3(colorspace::luma_coefficients, rgb);
}

uchar IMB_colormanagement_get_luminance_byte(const uchar rgb[3])
{
  float rgbf[3];
  float val;

  rgb_uchar_to_float(rgbf, rgb);
  val = dot_v3v3(colorspace::luma_coefficients, rgbf);

  return unit_float_to_uchar_clamp(val);
}

void IMB_colormanagement_xyz_to_scene_linear(float scene_linear[3], const float xyz[3])
{
  mul_v3_m3v3(scene_linear, colorspace::xyz_to_scene_linear.ptr(), xyz);
}

void IMB_colormanagement_scene_linear_to_xyz(float xyz[3], const float scene_linear[3])
{
  mul_v3_m3v3(xyz, colorspace::scene_linear_to_xyz.ptr(), scene_linear);
}

void IMB_colormanagement_rec709_to_scene_linear(float scene_linear[3], const float rec709[3])
{
  if (colorspace::scene_linear_is_rec709) {
    copy_v3_v3(scene_linear, rec709);
  }
  else {
    mul_v3_m3v3(scene_linear, colorspace::rec709_to_scene_linear.ptr(), rec709);
  }
}

void IMB_colormanagement_scene_linear_to_rec709(float rec709[3], const float scene_linear[3])
{
  if (colorspace::scene_linear_is_rec709) {
    copy_v3_v3(rec709, scene_linear);
  }
  else {
    mul_v3_m3v3(rec709, colorspace::scene_linear_to_rec709.ptr(), scene_linear);
  }
}

void IMB_colormanagement_scene_linear_to_srgb_v3(float srgb[3], const float scene_linear[3])
{
  if (colorspace::scene_linear_is_rec709) {
    copy_v3_v3(srgb, scene_linear);
  }
  else {
    mul_v3_m3v3(srgb, colorspace::scene_linear_to_rec709.ptr(), scene_linear);
  }
  linearrgb_to_srgb_v3_v3(srgb, srgb);
}

void IMB_colormanagement_srgb_to_scene_linear_v3(float scene_linear[3], const float srgb[3])
{
  srgb_to_linearrgb_v3_v3(scene_linear, srgb);
  if (!colorspace::scene_linear_is_rec709) {
    mul_m3_v3(colorspace::rec709_to_scene_linear.ptr(), scene_linear);
  }
}

void IMB_colormanagement_aces_to_scene_linear(float scene_linear[3], const float aces[3])
{
  mul_v3_m3v3(scene_linear, colorspace::aces_to_scene_linear.ptr(), aces);
}

void IMB_colormanagement_scene_linear_to_aces(float aces[3], const float scene_linear[3])
{
  mul_v3_m3v3(aces, colorspace::scene_linear_to_aces.ptr(), scene_linear);
}

void IMB_colormanagement_acescg_to_scene_linear(float scene_linear[3], const float acescg[3])
{
  mul_v3_m3v3(scene_linear, colorspace::acescg_to_scene_linear.ptr(), acescg);
}

void IMB_colormanagement_scene_linear_to_acescg(float acescg[3], const float scene_linear[3])
{
  mul_v3_m3v3(acescg, colorspace::scene_linear_to_acescg.ptr(), scene_linear);
}

void IMB_colormanagement_rec2020_to_scene_linear(float scene_linear[3], const float rec2020[3])
{
  mul_v3_m3v3(scene_linear, colorspace::rec2020_to_scene_linear.ptr(), rec2020);
}

void IMB_colormanagement_scene_linear_to_rec2020(float rec2020[3], const float scene_linear[3])
{
  mul_v3_m3v3(rec2020, colorspace::scene_linear_to_rec2020.ptr(), scene_linear);
}

}  // namespace blender
