/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "draw_colormanagement_lib.glsl"

/* Keep in sync with image_engine.c */
#define IMAGE_DRAW_FLAG_SHOW_ALPHA (1 << 0)
#define IMAGE_DRAW_FLAG_APPLY_ALPHA (1 << 1)
#define IMAGE_DRAW_FLAG_SHUFFLING (1 << 2)
#define IMAGE_DRAW_FLAG_DEPTH (1 << 3)

#define FAR_DISTANCE far_near_distances.x
#define NEAR_DISTANCE far_near_distances.y

void main()
{
  int2 uvs_clamped = int2(uv_screen);
  float depth = texelFetch(depth_tx, uvs_clamped, 0).r;
  if (depth == 1.0f) {
    gpu_discard_fragment();
    return;
  }

  float4 tex_color = texelFetch(image_tx, uvs_clamped - offset, 0);

  if ((draw_flags & IMAGE_DRAW_FLAG_APPLY_ALPHA) != 0) {
    if (!is_image_premultiplied) {
      tex_color.rgb *= tex_color.a;
    }
  }
  if ((draw_flags & IMAGE_DRAW_FLAG_DEPTH) != 0) {
    tex_color = smoothstep(FAR_DISTANCE, NEAR_DISTANCE, tex_color);
  }

  if ((draw_flags & IMAGE_DRAW_FLAG_SHUFFLING) != 0) {
    tex_color = float4(dot(tex_color, shuffle));
  }
  if ((draw_flags & IMAGE_DRAW_FLAG_SHOW_ALPHA) == 0) {
    tex_color.a = 1.0f;
  }
  out_color = tex_color;
}
