/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void discard_vert()
{
  /* We set the vertex at the camera origin to generate 0 fragments. */
  gl_Position = vec4(0.0, 0.0, -3e36, 0.0);
}

#ifdef USE_POINTS
#  define gp_colorUnselect colorGpencilVertex
#  define gp_colorSelect colorGpencilVertexSelect
#else
#  define gp_colorUnselect gpEditColor
#  define gp_colorSelect (hideSelect ? gp_colorUnselect : colorGpencilVertexSelect)
#endif

vec3 weight_to_rgb(float t)
{
  if (t < 0.0) {
    /* No weight */
    return gp_colorUnselect.rgb;
  }
  else if (t > 1.0) {
    /* Error color */
    return vec3(1.0, 0.0, 1.0);
  }
  else {
    return texture(weightTex, t).rgb;
  }
}

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);

  bool is_multiframe = (vflag & GP_EDIT_MULTIFRAME) != 0u;
  bool is_stroke_sel = (vflag & GP_EDIT_STROKE_SELECTED) != 0u;
  bool is_point_sel = (vflag & GP_EDIT_POINT_SELECTED) != 0u;
  bool is_point_dimmed = (vflag & GP_EDIT_POINT_DIMMED) != 0u;

  if (doWeightColor) {
    finalColor.rgb = weight_to_rgb(weight);
    finalColor.a = gpEditOpacity;
  }
  else {
    finalColor = (is_point_sel) ? gp_colorSelect : gp_colorUnselect;
    finalColor.a *= gpEditOpacity;
  }

#ifdef USE_POINTS
  gl_PointSize = sizeVertexGpencil * 2.0;

  if (is_point_dimmed) {
    finalColor.rgb = clamp(gp_colorUnselect.rgb + vec3(0.3), 0.0, 1.0);
  }

  if (doStrokeEndpoints && !doWeightColor) {
    bool is_stroke_start = (vflag & GP_EDIT_STROKE_START) != 0u;
    bool is_stroke_end = (vflag & GP_EDIT_STROKE_END) != 0u;

    if (is_stroke_start) {
      gl_PointSize *= 2.0;
      finalColor.rgb = vec3(0.0, 1.0, 0.0);
    }
    else if (is_stroke_end) {
      gl_PointSize *= 1.5;
      finalColor.rgb = vec3(1.0, 0.0, 0.0);
    }
  }

  if ((!is_stroke_sel && !doWeightColor) || (!doMultiframe && is_multiframe)) {
    discard_vert();
  }
#endif

  /* Discard unwanted padding vertices. */
  if (ma == -1 || (is_multiframe && !doMultiframe)) {
    discard_vert();
  }

  view_clipping_distances(world_pos);
}
