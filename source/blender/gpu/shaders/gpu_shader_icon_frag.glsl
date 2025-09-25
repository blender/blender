/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Draw the icons, leaving a semi-transparent rectangle on top of the icon.
 *
 * The top-left corner of the rectangle is rounded and drawn with anti-alias.
 * The anti-alias is done by transitioning from the outer to the inner radius of
 * the rounded corner, and the rectangle sides.
 */

#include "infos/gpu_shader_icon_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_icon)

void main()
{
  /* Sample texture with LOD BIAS. Used instead of custom LOD bias in GPU_SAMPLER_CUSTOM_ICON. */
  fragColor = texture(image, texCoord_interp, -0.5f) * final_color;

  if (do_corner_masking) {
    /* Top-left rounded corner parameters. */
    constexpr float circle_radius_outer = 0.1f;
    constexpr float circle_radius_inner = 0.075f;

    /**
     * Add a bit transparency to see a bit of the icon, without
     * getting on the way of readability. */
    constexpr float mask_transparency = 0.25f;

    float2 circle_center = float2(circle_radius_outer - text_width, 0.5f);

    /* Radius in icon space (1 is the icon width). */
    float radius = length(mask_coord_interp - circle_center);
    float mask = smoothstep(circle_radius_inner, circle_radius_outer, radius);

    bool lower_half = mask_coord_interp.y < circle_center.y;
    bool right_half = mask_coord_interp.x > circle_center.x;

    if (right_half && mask_coord_interp.y < circle_center.y + circle_radius_outer) {
      mask = smoothstep(circle_center.y + circle_radius_inner,
                        circle_center.y + circle_radius_outer,
                        mask_coord_interp.y);
    }
    if (lower_half && mask_coord_interp.x > circle_center.x - circle_radius_outer) {
      mask = smoothstep(circle_center.x - circle_radius_inner,
                        circle_center.x - circle_radius_outer,
                        mask_coord_interp.x);
    }

    fragColor = mix(float4(0.0f), fragColor, max(mask_transparency, mask));
  }
}
