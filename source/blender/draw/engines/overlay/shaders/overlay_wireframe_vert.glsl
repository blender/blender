/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_wireframe_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_wireframe)

#include "draw_model_lib.glsl"
#include "draw_object_infos_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "gpu_shader_math_vector_safe_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"
#include "overlay_common_lib.glsl"
#include "select_lib.glsl"

#if !defined(POINTS) && !defined(CURVES)
bool is_edge_sharpness_visible(float wire_data)
{
  return wire_data <= wire_step_param;
}
#endif

void wire_color_get(out float3 rim_col, out float3 wire_col)
{
  eObjectInfoFlag ob_flag = drw_object_infos().flag;
  bool is_selected = flag_test(ob_flag, OBJECT_SELECTED);
  bool is_from_set = flag_test(ob_flag, OBJECT_FROM_SET);
  bool is_active = flag_test(ob_flag, OBJECT_ACTIVE);

  if (is_from_set) {
    rim_col = theme.colors.wire.rgb;
    wire_col = theme.colors.wire.rgb;
  }
  else if (is_selected && use_coloring) {
    if (is_transform) {
      rim_col = theme.colors.transform.rgb;
    }
    else if (is_active) {
      rim_col = theme.colors.active_object.rgb;
    }
    else {
      rim_col = theme.colors.object_select.rgb;
    }
    wire_col = theme.colors.wire.rgb;
  }
  else {
    rim_col = theme.colors.wire.rgb;
    wire_col = theme.colors.background.rgb;
  }
}

float3 hsv_to_rgb(float3 hsv)
{
  float3 nrgb = abs(hsv.x * 6.0f - float3(3.0f, 2.0f, 4.0f)) * float3(1, -1, -1) +
                float3(-1, 2, 2);
  nrgb = clamp(nrgb, 0.0f, 1.0f);
  return ((nrgb - 1.0f) * hsv.y + 1.0f) * hsv.z;
}

void wire_object_color_get(out float3 rim_col, out float3 wire_col)
{
  ObjectInfos info = drw_object_infos();
  bool is_selected = flag_test(info.flag, OBJECT_SELECTED);

  if (color_type == V3D_SHADING_OBJECT_COLOR) {
    rim_col = wire_col = drw_object_infos().ob_color.rgb * 0.5f;
  }
  else {
    float hue = info.random;
    float3 hsv = float3(hue, 0.75f, 0.8f);
    rim_col = wire_col = hsv_to_rgb(hsv);
  }

  if (is_selected && use_coloring) {
    /* "Normalize" color. */
    wire_col += 1e-4f; /* Avoid division by 0. */
    float brightness = max(wire_col.x, max(wire_col.y, wire_col.z));
    wire_col *= 0.5f / brightness;
    rim_col += 0.75f;
  }
  else {
    rim_col *= 0.5f;
    wire_col += 0.5f;
  }
}

void main()
{
  select_id_set(drw_custom_id());

  /* If no attribute is available, use a fixed facing value depending on the coloring mode.
   * This allow to keep most of the contrast between unselected and selected color
   * while keeping object coloring mode working (see #134011). */
  float no_nor_facing = (color_type == V3D_SHADING_SINGLE_COLOR) ? 0.0f : 0.5f;

  float3 wpos = drw_point_object_to_world(pos);
#if defined(POINTS)
  gl_PointSize = theme.sizes.vert * 2.0f;
#elif defined(CURVES)
  float facing = no_nor_facing;
#else
  float3 wnor = safe_normalize(drw_normal_object_to_world(nor));

  if (is_hair) {
    float4x4 obmat = hair_dupli_matrix;
    wpos = (obmat * float4(pos, 1.0f)).xyz;
    wnor = -normalize(to_float3x3(obmat) * nor);
  }

  bool is_persp = (drw_view().winmat[3][3] == 0.0f);
  float3 V = (is_persp) ? normalize(drw_view().viewinv[3].xyz - wpos) : drw_view().viewinv[2].xyz;

  bool no_attr = all(equal(nor, float3(0)));
  float facing = no_attr ? no_nor_facing : dot(wnor, V);
#endif

  gl_Position = drw_point_world_to_homogenous(wpos);

#if !defined(POINTS) && !defined(CURVES)
  if (!use_custom_depth_bias) {
    float facing_ratio = clamp(1.0f - facing * facing, 0.0f, 1.0f);
    float flip = sign(facing); /* Flip when not facing the normal (i.e.: back-facing). */
    float curvature = (1.0f - wd * 0.75f); /* Avoid making things worse for curvy areas. */
    float3 wofs = wnor * (facing_ratio * curvature * flip);
    wofs = drw_normal_world_to_view(wofs);

    /* Push vertex half a pixel (maximum) in normal direction. */
    gl_Position.xy += wofs.xy * uniform_buf.size_viewport_inv * gl_Position.w;

    /* Push the vertex towards the camera. Helps a bit. */
    gl_Position.z -= facing_ratio * curvature * 1.0e-6f * gl_Position.w;
  }
#endif

  /* Curves do not need the offset since they *are* the curve geometry. */
#if !defined(CURVES)
  gl_Position.z -= ndc_offset_factor * 0.5f;
#endif

  float3 rim_col, wire_col;
  if (color_type == V3D_SHADING_OBJECT_COLOR || color_type == V3D_SHADING_RANDOM_COLOR) {
    wire_object_color_get(rim_col, wire_col);
  }
  else {
    wire_color_get(rim_col, wire_col);
  }

#if defined(POINTS)
  final_color = wire_col.rgbb;
  final_color_inner = rim_col.rgbb;

#else
  /* Convert to screen position [0..sizeVp]. */
  edge_start = ((gl_Position.xy / gl_Position.w) * 0.5f + 0.5f) * uniform_buf.size_viewport;
  edge_pos = edge_start;

#  if !defined(SELECT_ENABLE)
  facing = clamp(abs(facing), 0.0f, 1.0f);
  /* Do interpolation in a non-linear space to have a better visual result. */
  rim_col = pow(rim_col, float3(1.0f / 2.2f));
  wire_col = pow(wire_col, float3(1.0f / 2.2f));
  float3 final_front_col = mix(rim_col, wire_col, 0.35f);
  final_color.rgb = mix(rim_col, final_front_col, facing);
  final_color.rgb = pow(final_color.rgb, float3(2.2f));
#  endif

  final_color.a = wire_opacity;
  final_color.rgb *= wire_opacity;

#  if !defined(CURVES)
  /* Cull flat edges below threshold. */
  if (!no_attr && !is_edge_sharpness_visible(wd)) {
    edge_start = float2(-1.0f);
  }
#  endif

#  if defined(SELECT_ENABLE)
  /* HACK: to avoid losing sub-pixel object in selections, we add a bit of randomness to the
   * wire to at least create one fragment that will pass the occlusion query. */
  gl_Position.xy += uniform_buf.size_viewport_inv * gl_Position.w *
                    ((gl_VertexID % 2 == 0) ? -1.0f : 1.0f);
#  endif
#endif

  view_clipping_distances(wpos);
}
