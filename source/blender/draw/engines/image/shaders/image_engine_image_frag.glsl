/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/engine_image_infos.hh"

#include "gpu_shader_math_matrix_transform_lib.glsl"
#include "image_engine_lib.glsl"

void main()
{
  const float2 coordinates = transform_point(image_matrix, float3(screen_uv, 0.0f)).xy();
  if (!is_repeated &&
      (any(lessThan(coordinates, float2(0.0))) || any(greaterThan(coordinates, float2(1.0)))))
  {
    out_color = float4(0.0f);
    gl_FragDepth = Z_DEPTH_BORDER;
    return;
  }

  const float4 image_color = texture(image_tx, coordinates);
  out_color = image_engine_apply_parameters(
      image_color, draw_flags, is_image_premultiplied, shuffle, FAR_DISTANCE, NEAR_DISTANCE);
  gl_FragDepth = Z_DEPTH_IMAGE;
}
