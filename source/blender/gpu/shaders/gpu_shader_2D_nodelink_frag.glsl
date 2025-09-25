/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_nodelink_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_2D_nodelink)

#define ANTIALIAS 0.75f

float get_line_alpha(float center, float relative_radius)
{
  float radius = relative_radius * interp_flat.line_thickness;
  float sdf = abs(interp_flat.line_thickness * (interp.line_uv.y - center));
  return smoothstep(radius, radius - ANTIALIAS, sdf);
}

void main()
{
  float dash_frag_alpha = 1.0f;
  if (interp_flat.dash_factor < 1.0f) {
    float distance_along_line = interp_flat.line_length * interp.line_uv.x;

    /* Checking if `normalized_distance <= interp.dash_factor` is already enough for a basic
     * dash, however we want to handle a nice anti-alias. */

    float dash_center = interp_flat.dash_length * interp_flat.dash_factor * 0.5f;
    float normalized_distance_triangle =
        1.0f -
        abs((fract((distance_along_line - dash_center) / interp_flat.dash_length)) * 2.0f - 1.0f);
    float t = interp_flat.aspect * ANTIALIAS / interp_flat.dash_length;
    float slope = 1.0f / (2.0f * t);

    float unclamped_alpha = 1.0f -
                            slope * (normalized_distance_triangle - interp_flat.dash_factor + t);
    float alpha = max(interp_flat.dash_alpha, min(unclamped_alpha, 1.0f));

    dash_frag_alpha = alpha;
  }

  if (interp_flat.is_main_line == 0) {
    out_color = interp.final_color;
    out_color.a *= get_line_alpha(0.5f, 0.5f) * dash_frag_alpha;
    return;
  }

  if (interp_flat.has_back_link == 0) {
    out_color = interp.final_color;
    out_color.a *= get_line_alpha(0.5f, 0.5f) * dash_frag_alpha;
  }
  else {
    /* Draw two links right next to each other, the main link and the back-link. */
    float4 main_link_color = interp.final_color;
    main_link_color.a *= get_line_alpha(0.75f, 0.3f);

    float4 back_link_color = float4(float3(0.8f), 1.0f);
    back_link_color.a *= get_line_alpha(0.2f, 0.25f);

    /* Combine both links. */
    out_color.rgb = main_link_color.rgb * main_link_color.a +
                    back_link_color.rgb * back_link_color.a;
    out_color.a = main_link_color.a * dash_frag_alpha + back_link_color.a;
  }
}
