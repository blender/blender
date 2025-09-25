/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_extra_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_image_base)

#include "draw_colormanagement_lib.glsl"
#include "select_lib.glsl"

void main()
{
  float2 uvs_clamped = clamp(uvs, 0.0f, 1.0f);
  float4 tex_color;
  tex_color = texture_read_as_linearrgb(img_tx, img_premultiplied, uvs_clamped);

  frag_color = tex_color * ucolor;

  if (!img_alpha_blend) {
    /* Arbitrary discard anything below 5% opacity.
     * Note that this could be exposed to the User. */
    if (tex_color.a < 0.05f) {
      gpu_discard_fragment();
    }
    else {
      frag_color.a = 1.0f;
    }
  }

  /* Pre-multiplied blending. */
  frag_color.rgb *= frag_color.a;

  select_id_output(select_id);
}
