/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

#define no_active_weight 666.0

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
  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);
  float end_point_size_factor = 1.0f;

  if (useWeight) {
    finalColor = vec4(weight_to_rgb(selection), 1.0);
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
