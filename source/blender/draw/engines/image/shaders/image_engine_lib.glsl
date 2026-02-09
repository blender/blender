/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "image_shader_shared.hh"

#define Z_DEPTH_BORDER 1.0f
#define Z_DEPTH_IMAGE 0.75f

#define FAR_DISTANCE far_near_distances.x
#define NEAR_DISTANCE far_near_distances.y

float4 image_engine_apply_parameters(float4 color,
                                     int flags,
                                     bool is_image_premultiplied,
                                     float4 shuffle_color,
                                     float far_distance,
                                     float near_distance)
{
  float4 result = color;
  if ((flags & IMAGE_DRAW_FLAG_APPLY_ALPHA) != 0) {
    if (!is_image_premultiplied) {
      result.rgb *= result.a;
    }
  }
  if ((flags & IMAGE_DRAW_FLAG_DEPTH) != 0) {
    result = smoothstep(far_distance, near_distance, result);
  }

  if ((flags & IMAGE_DRAW_FLAG_SHUFFLING) != 0) {
    result = float4(dot(result, shuffle_color));
  }
  if ((flags & IMAGE_DRAW_FLAG_SHOW_ALPHA) == 0) {
    result.a = 1.0f;
  }
  return result;
}
