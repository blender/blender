/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/engine_image_infos.hh"

#include "draw_colormanagement_lib.glsl"
#include "image_engine_lib.glsl"

void main()
{
  int2 uvs_clamped = int2(uv_screen);
  float depth = texelFetch(depth_tx, uvs_clamped, 0).r;
  if (depth == 1.0f) {
    gpu_discard_fragment();
    return;
  }

  float4 tex_color = texelFetch(image_tx, uvs_clamped - offset, 0);

  out_color = image_engine_apply_parameters(
      tex_color, draw_flags, is_image_premultiplied, shuffle, FAR_DISTANCE, NEAR_DISTANCE);
}
