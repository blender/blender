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

  if (useWeight) {
    finalColor = vec4(weight_to_rgb(selection), 1.0);
  }
  else {
    vec4 use_color = useGreasePencil ? colorGpencilVertexSelect : colorVertexSelect;
    finalColor = mix(colorWire, use_color, selection);
  }

  float vsize = useGreasePencil ? sizeVertexGpencil : sizeVertex;
  gl_PointSize = vsize * 2.0;

  view_clipping_distances(world_pos);
}
