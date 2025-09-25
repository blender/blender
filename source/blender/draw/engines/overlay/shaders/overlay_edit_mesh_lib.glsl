/* SPDX-FileCopyrightText: 2017-2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/overlay_edit_mode_infos.hh"

SHADER_LIBRARY_CREATE_INFO(overlay_edit_mesh_common)
SHADER_LIBRARY_CREATE_INFO(draw_modelmat)

#include "draw_model_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"
#include "overlay_common_lib.glsl"
#include "overlay_edit_mesh_common_lib.glsl"

struct VertIn {
  /* Local Position. */
  float3 lP;
  /* Local Vertex Normal. */
  float3 lN;
  /* Edit Flags and Data. */
  uint4 e_data;
};

bool test_occlusion(float4 gpu_position)
{
  float3 ndc = (gpu_position.xyz / gpu_position.w) * 0.5f + 0.5f;
  return ndc.z > texture(depth_tx, ndc.xy).r;
}

float3 non_linear_blend_color(float3 col1, float3 col2, float fac)
{
  col1 = pow(col1, float3(1.0f / 2.2f));
  col2 = pow(col2, float3(1.0f / 2.2f));
  float3 col = mix(col1, col2, fac);
  return pow(col, float3(2.2f));
}

struct VertOut {
  float4 gpu_position;
  float4 final_color;
  float4 final_color_outer;
  float3 world_position;
  uint select_override;
};

VertOut vertex_main(VertIn vert_in)
{
  VertOut vert_out;

  vert_out.world_position = drw_point_object_to_world(vert_in.lP);
  float3 view_pos = drw_point_world_to_view(vert_out.world_position);
  vert_out.gpu_position = drw_point_view_to_homogenous(view_pos);

  /* Offset Z position for retopology overlay. */
  vert_out.gpu_position.z += get_homogenous_z_offset(
      drw_view().winmat, view_pos.z, vert_out.gpu_position.w, retopology_offset);

  uint4 m_data = vert_in.e_data & uint4(data_mask);

#if defined(VERT)
  vertex_crease = float(m_data.z >> 4) / 15.0f;
  vert_out.final_color = EDIT_MESH_vertex_color(m_data.y, vertex_crease);
  gl_PointSize = theme.sizes.vert * ((vertex_crease > 0.0f) ? 3.0f : 2.0f);
  /* Make selected and active vertex always on top. */
  if ((data.x & VERT_SELECTED) != 0u) {
    vert_out.gpu_position.z -= 5e-7f * abs(vert_out.gpu_position.w);
  }
  if ((data.x & VERT_ACTIVE) != 0u) {
    vert_out.gpu_position.z -= 5e-7f * abs(vert_out.gpu_position.w);
  }

  bool occluded = test_occlusion(vert_out.gpu_position);

#elif defined(EDGE)
  if (use_vertex_selection) {
    vert_out.final_color = EDIT_MESH_edge_vertex_color(m_data.y);
    vert_out.select_override = (m_data.y & EDGE_SELECTED);
  }
  else {
    vert_out.final_color = EDIT_MESH_edge_color_inner(m_data.y);
    vert_out.select_override = 1u;
  }

  float edge_crease = float(m_data.z & 0xFu) / 15.0f;
  float bweight = float(m_data.w) / 255.0f;
  vert_out.final_color_outer = EDIT_MESH_edge_color_outer(
      m_data.y, m_data.x, edge_crease, bweight);

  if (vert_out.final_color_outer.a > 0.0f) {
    vert_out.gpu_position.z -= 5e-7f * abs(vert_out.gpu_position.w);
  }

  bool occluded = false; /* Done in fragment shader */

#elif defined(FACE)
  vert_out.final_color = EDIT_MESH_face_color(m_data.x);
  bool occluded = true;

#  ifdef GPU_METAL
  /* Apply depth bias to overlay in order to prevent z-fighting on Apple Silicon GPUs. */
  vert_out.gpu_position.z -= 5e-5f;
#  endif

#elif defined(FACEDOT)
  vert_out.final_color = EDIT_MESH_facedot_color(norAndFlag.w);

  /* Bias Face-dot Z position in clip-space. */
  vert_out.gpu_position.z -= (drw_view().winmat[3][3] == 0.0f) ? 0.00035f : 1e-6f;

  bool occluded = test_occlusion(vert_out.gpu_position);

  gl_PointSize = theme.sizes.face_dot;

#  ifdef GLSL_CPP_STUBS
  /* Fixes warning in C++ compilation about unused variable. */
  vert_out.gpu_position.z = m_data.x;
#  endif
#endif

  vert_out.final_color.a *= (occluded) ? alpha : 1.0f;

#if !defined(FACE)
  /* Facing based color blend */
  float3 view_normal = normalize(drw_normal_object_to_view(vert_in.lN) + 1e-4f);
  float3 view_vec = (drw_view().winmat[3][3] == 0.0f) ? normalize(view_pos) :
                                                        float3(0.0f, 0.0f, 1.0f);
  float facing = dot(view_vec, view_normal);
  facing = 1.0f - abs(facing) * 0.2f;

  /* Do interpolation in a non-linear space to have a better visual result. */
  vert_out.final_color.rgb = mix(
      vert_out.final_color.rgb,
      non_linear_blend_color(theme.colors.edit_mesh_middle.rgb, vert_out.final_color.rgb, facing),
      theme.fresnel_mix_edit);
#endif

  vert_out.gpu_position.z -= ndc_offset_factor * ndc_offset;

  return vert_out;
}
