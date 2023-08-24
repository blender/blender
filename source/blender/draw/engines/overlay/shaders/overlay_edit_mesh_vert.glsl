/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(overlay_edit_mesh_common_lib.glsl)

#ifdef EDGE
/* Ugly but needed to keep the same vertex shader code for other passes. */
#  define finalColor geometry_in.finalColor_
#  define finalColorOuter geometry_in.finalColorOuter_
#  define selectOverride geometry_in.selectOverride_
#endif

bool test_occlusion()
{
  vec3 ndc = (gl_Position.xyz / gl_Position.w) * 0.5 + 0.5;
  return ndc.z > texture(depthTex, ndc.xy).r;
}

vec3 non_linear_blend_color(vec3 col1, vec3 col2, float fac)
{
  col1 = pow(col1, vec3(1.0 / 2.2));
  col2 = pow(col2, vec3(1.0 / 2.2));
  vec3 col = mix(col1, col2, fac);
  return pow(col, vec3(2.2));
}

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  vec3 world_pos = point_object_to_world(pos);
  vec3 view_pos = point_world_to_view(world_pos);
  gl_Position = point_view_to_ndc(view_pos);

  /* Offset Z position for retopology overlay. */
  gl_Position.z += get_homogenous_z_offset(view_pos.z, gl_Position.w, retopologyOffset);

  uvec4 m_data = data & uvec4(dataMask);

#if defined(VERT)
  vertexCrease = float(m_data.z >> 4) / 15.0;
  finalColor = EDIT_MESH_vertex_color(m_data.y, vertexCrease);
  gl_PointSize = sizeVertex * ((vertexCrease > 0.0) ? 3.0 : 2.0);
  /* Make selected and active vertex always on top. */
  if ((data.x & VERT_SELECTED) != 0u) {
    gl_Position.z -= 5e-7 * abs(gl_Position.w);
  }
  if ((data.x & VERT_ACTIVE) != 0u) {
    gl_Position.z -= 5e-7 * abs(gl_Position.w);
  }

  bool occluded = test_occlusion();

#elif defined(EDGE)
#  ifdef FLAT
  finalColor = EDIT_MESH_edge_color_inner(m_data.y);
  selectOverride = 1u;
#  else
  finalColor = EDIT_MESH_edge_vertex_color(m_data.y);
  selectOverride = (m_data.y & EDGE_SELECTED);
#  endif

  float edge_crease = float(m_data.z & 0xFu) / 15.0;
  float bweight = float(m_data.w) / 255.0;
  finalColorOuter = EDIT_MESH_edge_color_outer(m_data.y, m_data.x, edge_crease, bweight);

  if (finalColorOuter.a > 0.0) {
    gl_Position.z -= 5e-7 * abs(gl_Position.w);
  }

  bool occluded = false; /* Done in fragment shader */

#elif defined(FACE)
  finalColor = EDIT_MESH_face_color(m_data.x);
  bool occluded = true;

#  ifdef GPU_METAL
  /* Apply depth bias to overlay in order to prevent z-fighting on Apple Silicon GPUs. */
  gl_Position.z -= 5e-5;
#  endif

#elif defined(FACEDOT)
  finalColor = EDIT_MESH_facedot_color(norAndFlag.w);

  /* Bias Face-dot Z position in clip-space. */
  gl_Position.z -= (drw_view.winmat[3][3] == 0.0) ? 0.00035 : 1e-6;
  gl_PointSize = sizeFaceDot;

  bool occluded = test_occlusion();

#endif

  finalColor.a *= (occluded) ? alpha : 1.0;

#if !defined(FACE)
  /* Facing based color blend */
  vec3 view_normal = normalize(normal_object_to_view(vnor) + 1e-4);
  vec3 view_vec = (drw_view.winmat[3][3] == 0.0) ? normalize(view_pos) : vec3(0.0, 0.0, 1.0);
  float facing = dot(view_vec, view_normal);
  facing = 1.0 - abs(facing) * 0.2;

  /* Do interpolation in a non-linear space to have a better visual result. */
  finalColor.rgb = non_linear_blend_color(colorEditMeshMiddle.rgb, finalColor.rgb, facing);
#endif

  view_clipping_distances(world_pos);
}
