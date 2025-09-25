/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Merge overlays texture on top of image texture and transform to display space (assume sRGB) */

#include "infos/gpu_shader_2D_image_overlays_merge_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_2D_image_overlays_merge)

float linearrgb_to_srgb(float c)
{
  if (c < 0.0031308f) {
    return (c < 0.0f) ? 0.0f : c * 12.92f;
  }
  else {
    return 1.055f * pow(c, 1.0f / 2.4f) - 0.055f;
  }
}

void linearrgb_to_srgb(float4 col_from, out float4 col_to)
{
  col_to.r = linearrgb_to_srgb(col_from.r);
  col_to.g = linearrgb_to_srgb(col_from.g);
  col_to.b = linearrgb_to_srgb(col_from.b);
  col_to.a = col_from.a;
}

void main()
{
  fragColor = texture(image_texture, texCoord_interp.xy);
  float4 overlay_col = texture(overlays_texture, texCoord_interp.xy);

  if (overlay) {
    if (use_hdr_display) {
      /* When using HDR, interpolate towards clamped color to improve display of
       * alpha-blended overlays. */
      fragColor = mix(fragColor, clamp(fragColor, 0.0f, 1.0f), overlay_col.a);
    }
    fragColor *= 1.0f - overlay_col.a;
    fragColor += overlay_col;
  }

  if (display_transform) {
    linearrgb_to_srgb(fragColor, fragColor);
  }
}
