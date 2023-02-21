
#pragma USE_SSBO_VERTEX_FETCH(TriangleList, 6)

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(overlay_edit_mesh_common_lib.glsl)

#define DISCARD_VERTEX \
  gl_Position = geometry_out.finalColorOuter = geometry_out.finalColor = vec4(0.0); \
  geometry_out.edgeCoord = 0.0; \
  return;

bool test_occlusion(vec4 pos)
{
  vec3 ndc = (pos.xyz / pos.w) * 0.5 + 0.5;
  return ndc.z > texture(depthTex, ndc.xy).r;
}

vec3 non_linear_blend_color(vec3 col1, vec3 col2, float fac)
{
  col1 = pow(col1, vec3(1.0 / 2.2));
  col2 = pow(col2, vec3(1.0 / 2.2));
  vec3 col = mix(col1, col2, fac);
  return pow(col, vec3(2.2));
}

vec3 vec3_1010102_Inorm_to_vec3(vec3 data)
{
  return data;
}

vec3 vec3_1010102_Inorm_to_vec3(int data)
{
  vec3 out_vec;
  out_vec.x = float(clamp(data, -512, 511)) / 511.0f;
  out_vec.y = float(clamp(data >> 10, -512, 511)) / 511.0f;
  out_vec.z = float(clamp(data >> 20, -512, 511)) / 511.0f;
  return out_vec;
}

void do_vertex(vec4 color, vec4 pos, float coord, vec2 offset)
{
  geometry_out.finalColor = color;
  geometry_out.edgeCoord = coord;
  gl_Position = pos;
  /* Multiply offset by 2 because gl_Position range is [-1..1]. */
  gl_Position.xy += offset * 2.0 * pos.w;
}

void main()
{
  /* Index of the quad primitive -- corresponds to one line prim. */
  int quad_id = gl_VertexID / 6;

  /* Determine vertex within the output 2-triangle quad (A, B, C)(A, C, D). */
  int quad_vertex_id = gl_VertexID % 6;

  /* Base index of the line primitive:
   * IF PrimType == LineList:  base_vertex_id = quad_id*2
   * IF PrimType == LineStrip: base_vertex_id = quad_id
   *
   * NOTE: This is currently used as LineList.
   *
   * NOTE: Primitive Restart Will not work with this setup as-is. We should avoid using
   * input primitive types which use restart indices. */
  int base_vertex_id = quad_id * 2;

  /* Fetch attribute values for self and neighboring vertex. */
  vec3 in_pos0 = vertex_fetch_attribute(base_vertex_id, pos, vec3);
  vec3 in_pos1 = vertex_fetch_attribute(base_vertex_id + 1, pos, vec3);
  uchar4 in_data0 = vertex_fetch_attribute(base_vertex_id, data, uchar4);
  uchar4 in_data1 = vertex_fetch_attribute(base_vertex_id + 1, data, uchar4);
  vec3 in_vnor0 = vec3_1010102_Inorm_to_vec3(
      vertex_fetch_attribute(base_vertex_id, vnor, vec3_1010102_Inorm));
  vec3 in_vnor1 = vec3_1010102_Inorm_to_vec3(
      vertex_fetch_attribute(base_vertex_id + 1, vnor, vec3_1010102_Inorm));

  /* Calculate values for self and neighboring vertex. */
  vec4 out_finalColor[2];
  vec4 out_finalColorOuter[2];
  int selectOveride[2];

  vec3 world_pos0 = point_object_to_world(in_pos0);
  vec3 world_pos1 = point_object_to_world(in_pos1);
  vec4 out_pos0 = point_world_to_ndc(world_pos0);
  vec4 out_pos1 = point_world_to_ndc(world_pos1);
  uvec4 m_data0 = uvec4(in_data0) & uvec4(dataMask);
  uvec4 m_data1 = uvec4(in_data1) & uvec4(dataMask);

#if defined(EDGE)
#  ifdef FLAT
  out_finalColor[0] = EDIT_MESH_edge_color_inner(m_data0.y);
  out_finalColor[1] = EDIT_MESH_edge_color_inner(m_data1.y);
  selectOveride[0] = 1;
  selectOveride[1] = 1;
#  else
  out_finalColor[0] = EDIT_MESH_edge_vertex_color(m_data0.y);
  out_finalColor[1] = EDIT_MESH_edge_vertex_color(m_data1.y);
  selectOveride[0] = (m_data0.y & EDGE_SELECTED);
  selectOveride[1] = (m_data1.y & EDGE_SELECTED);
#  endif

  float crease0 = float(m_data0.z) / 255.0;
  float crease1 = float(m_data1.z) / 255.0;
  float bweight0 = float(m_data0.w) / 255.0;
  float bweight1 = float(m_data1.w) / 255.0;
  out_finalColorOuter[0] = EDIT_MESH_edge_color_outer(m_data0.y, m_data0.x, crease0, bweight0);
  out_finalColorOuter[1] = EDIT_MESH_edge_color_outer(m_data1.y, m_data1.x, crease1, bweight1);

  if (out_finalColorOuter[0].a > 0.0) {
    out_pos0.z -= 5e-7 * abs(out_pos0.w);
  }
  if (out_finalColorOuter[1].a > 0.0) {
    out_pos1.z -= 5e-7 * abs(out_pos1.w);
  }

  /* Occlusion done in fragment shader. */
  bool occluded0 = false;
  bool occluded1 = false;
#endif

  out_finalColor[0].a *= (occluded0) ? alpha : 1.0;
  out_finalColor[1].a *= (occluded1) ? alpha : 1.0;

#if !defined(FACE)
  /* Facing based color blend */
  vec3 vpos0 = point_world_to_view(world_pos0);
  vec3 view_normal0 = normalize(normal_object_to_view(in_vnor0) + 1e-4);
  vec3 view_vec0 = (ProjectionMatrix[3][3] == 0.0) ? normalize(vpos0) : vec3(0.0, 0.0, 1.0);
  float facing0 = dot(view_vec0, view_normal0);
  facing0 = 1.0 - abs(facing0) * 0.2;

  vec3 vpos1 = point_world_to_view(world_pos1);
  vec3 view_normal1 = normalize(normal_object_to_view(in_vnor1) + 1e-4);
  vec3 view_vec1 = (ProjectionMatrix[3][3] == 0.0) ? normalize(vpos1) : vec3(0.0, 0.0, 1.0);
  float facing1 = dot(view_vec1, view_normal1);
  facing1 = 1.0 - abs(facing1) * 0.2;

  /* Do interpolation in a non-linear space to have a better visual result. */
  out_finalColor[0].rgb = non_linear_blend_color(
      colorEditMeshMiddle.rgb, out_finalColor[0].rgb, facing0);
  out_finalColor[1].rgb = non_linear_blend_color(
      colorEditMeshMiddle.rgb, out_finalColor[1].rgb, facing1);
#endif

  // -------- GEOM SHADER ALTERNATIVE ----------- //
  vec2 ss_pos[2];

  /* Clip line against near plane to avoid deformed lines. */
  vec4 pos0 = out_pos0;
  vec4 pos1 = out_pos1;
  vec2 pz_ndc = vec2(pos0.z / pos0.w, pos1.z / pos1.w);
  bvec2 clipped = lessThan(pz_ndc, vec2(-1.0));
  if (all(clipped)) {
    /* Totally clipped. */
    DISCARD_VERTEX;
  }

  vec4 pos01 = pos0 - pos1;
  float ofs = abs((pz_ndc.y + 1.0) / (pz_ndc.x - pz_ndc.y));
  if (clipped.y) {
    pos1 += pos01 * ofs;
  }
  else if (clipped.x) {
    pos0 -= pos01 * (1.0 - ofs);
  }

  ss_pos[0] = pos0.xy / pos0.w;
  ss_pos[1] = pos1.xy / pos1.w;

  vec2 line = ss_pos[0] - ss_pos[1];
  line = abs(line) * sizeViewport.xy;

  geometry_out.finalColorOuter = out_finalColorOuter[0];
  float half_size = sizeEdge;
  /* Enlarge edge for flag display. */
  half_size += (geometry_out.finalColorOuter.a > 0.0) ? max(sizeEdge, 1.0) : 0.0;

#ifdef USE_SMOOTH_WIRE
  /* Add 1 px for AA */
  half_size += 0.5;
#endif

  vec3 edge_ofs = vec3(half_size * sizeViewportInv, 0.0);

  bool horizontal = line.x > line.y;
  edge_ofs = (horizontal) ? edge_ofs.zyz : edge_ofs.xzz;

  vec4 final_color = (selectOveride[0] == 0) ? out_finalColor[1] : out_finalColor[0];

  /* Output specific Vertex data depending on quad_vertex_id. */
  if (quad_vertex_id == 0) {
    view_clipping_distances(world_pos0);
    do_vertex(out_finalColor[0], pos0, half_size, edge_ofs.xy);
  }
  else if (quad_vertex_id == 1 || quad_vertex_id == 3) {
    view_clipping_distances(world_pos0);
    do_vertex(out_finalColor[0], pos0, -half_size, -edge_ofs.xy);
  }
  else if (quad_vertex_id == 2 || quad_vertex_id == 5) {
    view_clipping_distances(world_pos1);
    do_vertex(final_color, pos1, half_size, edge_ofs.xy);
  }
  else if (quad_vertex_id == 4) {
    view_clipping_distances(world_pos1);
    do_vertex(final_color, pos1, -half_size, -edge_ofs.xy);
  }
}
