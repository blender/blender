/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_edit_mode_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_edit_particle_point)

#include "draw_model_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"

#define no_active_weight 666.0f

#define DISCARD_VERTEX \
  gl_Position = float4(0.0f, 0.0f, -3e36f, 0.0f); \
  return;

float3 weight_to_rgb(float t)
{
  if (t == no_active_weight) {
    /* No weight. */
    return theme.colors.wire.rgb;
  }
  if (t > 1.0f || t < 0.0f) {
    /* Error color */
    return float3(1.0f, 0.0f, 1.0f);
  }
  else {
    return texture(weight_tx, t).rgb;
  }
}

void main()
{
#ifdef CURVES_POINT
  bool is_active = (data & EDIT_CURVES_ACTIVE_HANDLE) != 0u;
  bool is_bezier_handle = (data & EDIT_CURVES_BEZIER_HANDLE) != 0u;

  if (is_bezier_handle && ((uint(curve_handle_display) == CURVE_HANDLE_NONE) ||
                           (uint(curve_handle_display) == CURVE_HANDLE_SELECTED) && !is_active))
  {
    DISCARD_VERTEX
  }
#endif

  float3 world_pos = drw_point_object_to_world(pos);
  gl_Position = drw_point_world_to_homogenous(world_pos);
  float end_point_size_factor = 1.0f;

  if (use_weight) {
    final_color = float4(weight_to_rgb(selection), 1.0f);
  }
  else {
    float4 color_selected = use_grease_pencil ? theme.colors.gpencil_vertex_select :
                                                theme.colors.vert_select;
    float4 color_not_selected = use_grease_pencil ? theme.colors.gpencil_vertex :
                                                    theme.colors.vert;
    final_color = mix(color_not_selected, color_selected, selection);

#if 1 /* Should be checking CURVES_POINT */
    if (do_stroke_endpoints) {
      bool is_stroke_start = (vflag & GP_EDIT_STROKE_START) != 0u;
      bool is_stroke_end = (vflag & GP_EDIT_STROKE_END) != 0u;

      if (is_stroke_start) {
        end_point_size_factor *= 2.0f;
        final_color.rgb = float3(0.0f, 1.0f, 0.0f);
      }
      else if (is_stroke_end) {
        end_point_size_factor *= 1.5f;
        final_color.rgb = float3(1.0f, 0.0f, 0.0f);
      }
    }
#endif
  }

  float vsize = use_grease_pencil ? theme.sizes.vertex_gpencil : theme.sizes.vert;
  gl_PointSize = vsize * 2.0f * end_point_size_factor;

  view_clipping_distances(world_pos);
}
