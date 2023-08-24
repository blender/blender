/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Merge overlays texture on top of image texture and transform to display space (assume sRGB) */

#ifndef USE_GPU_SHADER_CREATE_INFO
uniform sampler2D image_texture;
uniform sampler2D overlays_texture;
uniform bool display_transform;
uniform bool overlay;

in vec2 texCoord_interp;

out vec4 fragColor;
#endif

float linearrgb_to_srgb(float c)
{
  if (c < 0.0031308) {
    return (c < 0.0) ? 0.0 : c * 12.92;
  }
  else {
    return 1.055 * pow(c, 1.0 / 2.4) - 0.055;
  }
}

void linearrgb_to_srgb(vec4 col_from, out vec4 col_to)
{
  col_to.r = linearrgb_to_srgb(col_from.r);
  col_to.g = linearrgb_to_srgb(col_from.g);
  col_to.b = linearrgb_to_srgb(col_from.b);
  col_to.a = col_from.a;
}

void main()
{
  fragColor = texture(image_texture, texCoord_interp.xy);
  vec4 overlay_col = texture(overlays_texture, texCoord_interp.xy);

  if (overlay) {
    if (!use_hdr) {
      /* If we're not using an extended colour space, clamp the color 0..1. */
      fragColor = clamp(fragColor, 0.0, 1.0);
    }
    else {
      /* When using extended colorspace, interpolate towards clamped color to improve display of
       * alpha-blended overlays. */
      fragColor = mix(max(fragColor, 0.0), clamp(fragColor, 0.0, 1.0), overlay_col.a);
    }
    fragColor *= 1.0 - overlay_col.a;
    fragColor += overlay_col;
  }

  if (display_transform) {
    linearrgb_to_srgb(fragColor, fragColor);
  }
}
