/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_view_clipping_lib.glsl"
#include "common_view_lib.glsl"

#define no_active_weight 666.0

#define DISCARD_VERTEX \
  gl_Position = vec4(0.0, 0.0, -3e36, 0.0); \
  return;

vec3 weight_to_rgb(float t)
{
  if (t == no_active_weight) {
    /* No weight. */
    return colorWire.rgb;
  }
  if (t > 1.0 || t < 0.0) {
    /* Error color */
    return vec3(1.0, 0.0, 1.0);
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

  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);
  float end_point_size_factor = 1.0f;

  if (useWeight) {
    finalColor = vec4(weight_to_rgb(selection), 1.0);
  }
  else {
    vec4 use_color = useGreasePencil ? colorGpencilVertexSelect : colorVertexSelect;
    finalColor = mix(colorWire, use_color, selection);

#if 1 /* Should be checking CURVES_POINT */
    if (doStrokeEndpoints) {
      bool is_stroke_start = (vflag & GP_EDIT_STROKE_START) != 0u;
      bool is_stroke_end = (vflag & GP_EDIT_STROKE_END) != 0u;

      if (is_stroke_start) {
        end_point_size_factor *= 2.0;
        finalColor.rgb = vec3(0.0, 1.0, 0.0);
      }
      else if (is_stroke_end) {
        end_point_size_factor *= 1.5;
        finalColor.rgb = vec3(1.0, 0.0, 0.0);
      }
    }
#endif
  }

  float vsize = useGreasePencil ? sizeVertexGpencil : sizeVertex;
  gl_PointSize = vsize * 2.0 * end_point_size_factor;

  view_clipping_distances(world_pos);
}
