/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_nodelink_info.hh"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_2D_nodelink)

#define ANTIALIAS 0.75f

float get_line_alpha(float center, float relative_radius)
{
  float radius = relative_radius * lineThickness;
  float sdf = abs(lineThickness * (lineUV.y - center));
  return smoothstep(radius, radius - ANTIALIAS, sdf);
}

void main()
{
  float dash_frag_alpha = 1.0f;
  if (dashFactor < 1.0f) {
    float distance_along_line = lineLength * lineUV.x;

    /* Checking if `normalized_distance <= dashFactor` is already enough for a basic
     * dash, however we want to handle a nice anti-alias. */

    float dash_center = dashLength * dashFactor * 0.5f;
    float normalized_distance_triangle =
        1.0f - abs((fract((distance_along_line - dash_center) / dashLength)) * 2.0f - 1.0f);
    float t = aspect * ANTIALIAS / dashLength;
    float slope = 1.0f / (2.0f * t);

    float unclamped_alpha = 1.0f - slope * (normalized_distance_triangle - dashFactor + t);
    float alpha = max(dashAlpha, min(unclamped_alpha, 1.0f));

    dash_frag_alpha = alpha;
  }

  if (isMainLine == 0) {
    fragColor = finalColor;
    fragColor.a *= get_line_alpha(0.5f, 0.5f) * dash_frag_alpha;
    return;
  }

  if (hasBackLink == 0) {
    fragColor = finalColor;
    fragColor.a *= get_line_alpha(0.5f, 0.5f) * dash_frag_alpha;
  }
  else {
    /* Draw two links right next to each other, the main link and the back-link. */
    float4 main_link_color = finalColor;
    main_link_color.a *= get_line_alpha(0.75f, 0.3f);

    float4 back_link_color = float4(float3(0.8f), 1.0f);
    back_link_color.a *= get_line_alpha(0.2f, 0.25f);

    /* Combine both links. */
    fragColor.rgb = main_link_color.rgb * main_link_color.a +
                    back_link_color.rgb * back_link_color.a;
    fragColor.a = main_link_color.a * dash_frag_alpha + back_link_color.a;
  }
}
