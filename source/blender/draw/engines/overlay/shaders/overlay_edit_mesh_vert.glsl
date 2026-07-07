/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_edit_mode_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_edit_mesh_vert)
#ifdef GLSL_CPP_STUBS
#  define VERT
#endif

#include "draw_model_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "overlay_common_lib.glsl"
#include "overlay_edit_mesh_common_lib.glsl"

#ifdef EDGE
/* Ugly but needed to keep the same vertex shader code for other passes. */
#  define final_color geometry_in.final_color_
#  define final_color_outer geometry_in.final_color_outer_
#  define selectOverride geometry_in.selectOverride_
#endif

bool test_occlusion()
{
  float3 ndc = (gl_Position.xyz / gl_Position.w) * 0.5f + 0.5f;
  float4 depths = textureGather(depth_tx, ndc.xy);
  return all(greaterThan(float4(ndc.z), depths));
}

float3 non_linear_blend_color(float3 col1, float3 col2, float fac)
{
  col1 = pow(col1, float3(1.0f / 2.2f));
  col2 = pow(col2, float3(1.0f / 2.2f));
  float3 col = mix(col1, col2, fac);
  return pow(col, float3(2.2f));
}

void main()
{
  float3 world_pos = drw_point_object_to_world(pos);
  float3 view_pos = drw_point_world_to_view(world_pos);
  gl_Position = drw_point_view_to_homogenous(view_pos);

  /* Offset Z position for retopology overlay. */
  gl_Position.z += get_homogenous_z_offset(
      drw_view().winmat, view_pos.z, gl_Position.w, retopology_offset);

  uint4 m_data = data & uint4(data_mask);

#if defined(VERT)
  vertex_crease = float(m_data.z >> 4) / 15.0f;
  final_color = EDIT_MESH_vertex_color(m_data.y, vertex_crease);
  gl_PointSize = theme.sizes.vert * ((vertex_crease > 0.0f) ? 3.0f : 2.0f);
  /* Make selected and active vertex always on top. */
  if ((data.x & VERT_SELECTED) != 0u) {
    gl_Position.z -= 5e-7f * abs(gl_Position.w);
  }
  if ((data.x & VERT_ACTIVE) != 0u) {
    gl_Position.z -= 5e-7f * abs(gl_Position.w);
  }

  bool occluded = test_occlusion();

#elif defined(EDGE)
#  ifdef FLAT
  final_color = EDIT_MESH_edge_color_inner(m_data.y);
  selectOverride = 1u;
#  else
  final_color = EDIT_MESH_edge_vertex_color(m_data.y);
  selectOverride = (m_data.y & EDGE_SELECTED);
#  endif

  float edge_crease = float(m_data.z & 0xFu) / 15.0f;
  float bweight = float(m_data.w) / 255.0f;
  final_color_outer = EDIT_MESH_edge_color_outer(m_data.y, m_data.x, edge_crease, bweight);

  if (final_color_outer.a > 0.0f) {
    gl_Position.z -= 5e-7f * abs(gl_Position.w);
  }

  bool occluded = false; /* Done in fragment shader */

#elif defined(FACE)
  final_color = EDIT_MESH_face_color(m_data.x);
  bool occluded = true;

#  ifdef GPU_METAL
  /* Apply depth bias to overlay in order to prevent z-fighting on Apple Silicon GPUs. */
  gl_Position.z -= 5e-5f;
#  endif

#elif defined(FACEDOT)
  final_color = EDIT_MESH_facedot_color(norAndFlag.w);

  /* Bias Face-dot Z position in clip-space. */
  gl_Position.z -= (drw_view().winmat[3][3] == 0.0f) ? 0.00035f : 1e-6f;
  gl_PointSize = theme.sizes.face_dot;

  bool occluded = test_occlusion();

#endif

  final_color.a *= (occluded) ? alpha : 1.0f;

#if !defined(FACE)
  /* Facing based color blend */
  float3 view_normal = normalize(drw_normal_object_to_view(vnor) + 1e-4f);
  float3 view_vec = (drw_view().winmat[3][3] == 0.0f) ? normalize(view_pos) :
                                                        float3(0.0f, 0.0f, 1.0f);
  float facing = dot(view_vec, view_normal);
  facing = 1.0f - abs(facing) * 0.2f;

  /* Do interpolation in a non-linear space to have a better visual result. */
  final_color.rgb = mix(
      final_color.rgb,
      non_linear_blend_color(theme.colors.edit_mesh_middle.rgb, final_color.rgb, facing),
      theme.fresnel_mix_edit);
#endif

  gl_Position.z -= ndc_offset_factor * ndc_offset;

  view_clipping_distances(world_pos);
}
