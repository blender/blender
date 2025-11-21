/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/* -------------------------------------------------------------------- */
/** \name Hardcoded color space conversion for fallback implementation
 *
 * NOTE: It is tempting to include gpu_shader_common_color_utils.glsl, but it should not be done
 * here as that header is intended to be used from the node shaders, and the source processor does
 * much more than simply including the file (it also follows some implicit dependencies that is
 * undesired here, and might break since we do not use node shaders here.
 * \{ */

float srgb_to_linear_rgb(float color)
{
  if (color < 0.04045f) {
    return (color < 0.0f) ? 0.0f : color * (1.0f / 12.92f);
  }
  return pow((color + 0.055f) * (1.0f / 1.055f), 2.4f);
}

float3 srgb_to_linear_rgb(float3 color)
{
  return float3(
      srgb_to_linear_rgb(color.r), srgb_to_linear_rgb(color.g), srgb_to_linear_rgb(color.b));
}

float linear_rgb_to_srgb(float color)
{
  if (color < 0.0031308f) {
    return (color < 0.0f) ? 0.0f : color * 12.92f;
  }

  return 1.055f * pow(color, 1.0f / 2.4f) - 0.055f;
}

float3 linear_rgb_to_srgb(float3 color)
{
  return float3(
      linear_rgb_to_srgb(color.r), linear_rgb_to_srgb(color.g), linear_rgb_to_srgb(color.b));
}

/** \} */
