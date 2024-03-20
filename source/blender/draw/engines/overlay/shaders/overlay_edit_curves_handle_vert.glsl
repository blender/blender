/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

float4 get_bezier_handle_color(uint color_id, float sel)
{
  switch (color_id) {
    case 0u: /* BEZIER_HANDLE_FREE */
      return mix(globalsBlock.color_handle_free, globalsBlock.color_handle_sel_free, sel);
    case 1u: /* BEZIER_HANDLE_AUTO */
      return mix(globalsBlock.color_handle_auto, globalsBlock.color_handle_sel_auto, sel);
    case 2u: /* BEZIER_HANDLE_VECTOR */
      return mix(globalsBlock.color_handle_vect, globalsBlock.color_handle_sel_vect, sel);
    case 3u: /* BEZIER_HANDLE_ALIGN */
      return mix(globalsBlock.color_handle_align, globalsBlock.color_handle_sel_align, sel);
  }
  return mix(globalsBlock.color_handle_autoclamp, globalsBlock.color_handle_sel_autoclamp, sel);
}

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);

  if ((data & EDIT_CURVES_BEZIER_HANDLE) != 0u) {
    leftColor = get_bezier_handle_color((data >> EDIT_CURVES_LEFT_HANDLE_TYPES_SHIFT) & 3,
                                        selection);
    finalColor = get_bezier_handle_color((data >> EDIT_CURVES_RIGHT_HANDLE_TYPES_SHIFT) & 3,
                                         selection);
  }
  else if ((data & EDIT_CURVES_NURBS_CONTROL_POINT) != 0u) {
    finalColor = mix(globalsBlock.color_nurb_uline, globalsBlock.color_nurb_sel_uline, selection);
  }
  else {
    finalColor = mix(colorWire, colorVertexSelect, selection);
  }

  view_clipping_distances(world_pos);
}
