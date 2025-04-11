/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_edit_mode_info.hh"

VERTEX_SHADER_CREATE_INFO(overlay_edit_particle_point)

#include "draw_model_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"

#define no_active_weight 666.0f

#define DISCARD_VERTEX \
  gl_Position = vec4(0.0f, 0.0f, -3e36f, 0.0f); \
  return;

vec3 weight_to_rgb(float t)
{
  if (t == no_active_weight) {
    /* No weight. */
    return colorWire.rgb;
  }
  if (t > 1.0f || t < 0.0f) {
    /* Error color */
    return vec3(1.0f, 0.0f, 1.0f);
  }
  else {
    return texture(weightTex, t).rgb;
  }
}

void main()
{
#ifdef CURVES_POINT
  bool is_active = (data & EDIT_CURVES_ACTIVE_HANDLE) != 0u;
  bool is_bezier_handle = (data & EDIT_CURVES_BEZIER_HANDLE) != 0u;

  if (is_bezier_handle && ((uint(curveHandleDisplay) == CURVE_HANDLE_NONE) ||
                           (uint(curveHandleDisplay) == CURVE_HANDLE_SELECTED) && !is_active))
  {
    DISCARD_VERTEX
  }
#endif

  vec3 world_pos = drw_point_object_to_world(pos);
  gl_Position = drw_point_world_to_homogenous(world_pos);
  float end_point_size_factor = 1.0f;

  if (useWeight) {
    finalColor = vec4(weight_to_rgb(selection), 1.0f);
  }
  else {
    vec4 color_selected = useGreasePencil ? colorGpencilVertexSelect : colorVertexSelect;
    vec4 color_not_selected = useGreasePencil ? colorGpencilVertex : colorVertex;
    finalColor = mix(color_not_selected, color_selected, selection);

#if 1 /* Should be checking CURVES_POINT */
    if (doStrokeEndpoints) {
      bool is_stroke_start = (vflag & GP_EDIT_STROKE_START) != 0u;
      bool is_stroke_end = (vflag & GP_EDIT_STROKE_END) != 0u;

      if (is_stroke_start) {
        end_point_size_factor *= 2.0f;
        finalColor.rgb = vec3(0.0f, 1.0f, 0.0f);
      }
      else if (is_stroke_end) {
        end_point_size_factor *= 1.5f;
        finalColor.rgb = vec3(1.0f, 0.0f, 0.0f);
      }
    }
#endif
  }

  float vsize = useGreasePencil ? sizeVertexGpencil : sizeVertex;
  gl_PointSize = vsize * 2.0f * end_point_size_factor;

  view_clipping_distances(world_pos);
}
