/* SPDX-FileCopyrightText: 2017-2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(overlay_edit_mesh_common_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_vector_lib.glsl)

struct VertIn {
  /* Local Position. */
  vec3 lP;
  /* Local Vertex Normal. */
  vec3 lN;
  /* Edit Flags and Data. */
  uvec4 e_data;
};

bool test_occlusion(vec4 gpu_position)
{
  vec3 ndc = (gpu_position.xyz / gpu_position.w) * 0.5 + 0.5;
  return ndc.z > texture(depthTex, ndc.xy).r;
}

vec3 non_linear_blend_color(vec3 col1, vec3 col2, float fac)
{
  col1 = pow(col1, vec3(1.0 / 2.2));
  col2 = pow(col2, vec3(1.0 / 2.2));
  vec3 col = mix(col1, col2, fac);
  return pow(col, vec3(2.2));
}

struct VertOut {
  vec4 gpu_position;
  vec4 final_color;
  vec4 final_color_outer;
  uint select_override;
};

VertOut vertex_main(VertIn vert_in)
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  VertOut vert_out;

  vec3 world_pos = point_object_to_world(vert_in.lP);
  vec3 view_pos = point_world_to_view(world_pos);
  vert_out.gpu_position = point_view_to_ndc(view_pos);

  /* Offset Z position for retopology overlay. */
  vert_out.gpu_position.z += get_homogenous_z_offset(
      view_pos.z, vert_out.gpu_position.w, retopologyOffset);

  uvec4 m_data = vert_in.e_data & uvec4(dataMask);

#if defined(VERT)
  vertexCrease = float(m_data.z >> 4) / 15.0;
  vert_out.final_color = EDIT_MESH_vertex_color(m_data.y, vertexCrease);
  gl_PointSize = sizeVertex * ((vertexCrease > 0.0) ? 3.0 : 2.0);
  /* Make selected and active vertex always on top. */
  if ((data.x & VERT_SELECTED) != 0u) {
    vert_out.gpu_position.z -= 5e-7 * abs(vert_out.gpu_position.w);
  }
  if ((data.x & VERT_ACTIVE) != 0u) {
    vert_out.gpu_position.z -= 5e-7 * abs(vert_out.gpu_position.w);
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

  float edge_crease = float(m_data.z & 0xFu) / 15.0;
  float bweight = float(m_data.w) / 255.0;
  vert_out.final_color_outer = EDIT_MESH_edge_color_outer(
      m_data.y, m_data.x, edge_crease, bweight);

  if (vert_out.final_color_outer.a > 0.0) {
    vert_out.gpu_position.z -= 5e-7 * abs(vert_out.gpu_position.w);
  }

  bool occluded = false; /* Done in fragment shader */

#elif defined(FACE)
  vert_out.final_color = EDIT_MESH_face_color(m_data.x);
  bool occluded = true;

#  ifdef GPU_METAL
  /* Apply depth bias to overlay in order to prevent z-fighting on Apple Silicon GPUs. */
  vert_out.gpu_position.z -= 5e-5;
#  endif

#elif defined(FACEDOT)
  vert_out.final_color = EDIT_MESH_facedot_color(norAndFlag.w);

  /* Bias Face-dot Z position in clip-space. */
  vert_out.gpu_position.z -= (drw_view.winmat[3][3] == 0.0) ? 0.00035 : 1e-6;

  bool occluded = test_occlusion(vert_out.gpu_position);

  gl_PointSize = sizeFaceDot;
#endif

  vert_out.final_color.a *= (occluded) ? alpha : 1.0;

#if !defined(FACE)
  /* Facing based color blend */
  vec3 view_normal = normalize(normal_object_to_view(vert_in.lN) + 1e-4);
  vec3 view_vec = (drw_view.winmat[3][3] == 0.0) ? normalize(view_pos) : vec3(0.0, 0.0, 1.0);
  float facing = dot(view_vec, view_normal);
  facing = 1.0 - abs(facing) * 0.2;

  /* Do interpolation in a non-linear space to have a better visual result. */
  vert_out.final_color.rgb = mix(
      vert_out.final_color.rgb,
      non_linear_blend_color(colorEditMeshMiddle.rgb, vert_out.final_color.rgb, facing),
      fresnelMixEdit);
#endif

  view_clipping_distances(world_pos);

  return vert_out;
}
