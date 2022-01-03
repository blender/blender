
/* To be compile with common_subdiv_lib.glsl */

layout(std430, binding = 0) readonly buffer inputVertexData
{
  PosNorLoop pos_nor[];
};

layout(std430, binding = 1) readonly buffer inputEdgeIndex
{
  uint input_edge_index[];
};

layout(std430, binding = 2) writeonly buffer outputEdgeFactors
{
#ifdef GPU_AMD_DRIVER_BYTE_BUG
  float output_edge_fac[];
#else
  uint output_edge_fac[];
#endif
};

void write_vec4(uint index, vec4 edge_facs)
{
#ifdef GPU_AMD_DRIVER_BYTE_BUG
  for (uint i = 0; i < 4; i++) {
    output_edge_fac[index + i] = edge_facs[i];
  }
#else
  /* Use same scaling as in extract_edge_fac_iter_poly_mesh. */
  uint a = uint(clamp(edge_facs.x * 253.0 + 1.0, 0.0, 255.0));
  uint b = uint(clamp(edge_facs.y * 253.0 + 1.0, 0.0, 255.0));
  uint c = uint(clamp(edge_facs.z * 253.0 + 1.0, 0.0, 255.0));
  uint d = uint(clamp(edge_facs.w * 253.0 + 1.0, 0.0, 255.0));
  uint packed_edge_fac = a << 24 | b << 16 | c << 8 | d;
  output_edge_fac[index] = packed_edge_fac;
#endif
}

/* From extract_mesh_vbo_edge_fac.cc, keep in sync! */
float loop_edge_factor_get(vec3 f_no, vec3 v_co, vec3 v_no, vec3 v_next_co)
{
  vec3 evec = v_next_co - v_co;
  vec3 enor = normalize(cross(v_no, evec));
  float d = abs(dot(enor, f_no));
  /* Re-scale to the slider range. */
  d *= (1.0 / 0.065);
  return clamp(d, 0.0, 1.0);
}

float compute_line_factor(uint start_loop_index, uint corner_index, vec3 face_normal)
{
  uint vertex_index = start_loop_index + corner_index;
  uint edge_index = input_edge_index[vertex_index];

  if (edge_index == -1 && optimal_display) {
    return 0.0;
  }

  /* Mod 4 so we loop back at the first vertex on the last loop index (3), but only the corner
   * index needs to be wrapped. */
  uint next_vertex_index = start_loop_index + (corner_index + 1) % 4;
  vec3 vertex_pos = get_vertex_pos(pos_nor[vertex_index]);
  vec3 vertex_nor = get_vertex_nor(pos_nor[vertex_index]);
  vec3 next_vertex_pos = get_vertex_pos(pos_nor[next_vertex_index]);
  return loop_edge_factor_get(face_normal, vertex_pos, vertex_nor, next_vertex_pos);
}

void main()
{
  /* We execute for each quad. */
  uint quad_index = get_global_invocation_index();
  if (quad_index >= total_dispatch_size) {
    return;
  }

  /* The start index of the loop is quad_index * 4. */
  uint start_loop_index = quad_index * 4;

  /* First compute the face normal, we need it to compute the bihedral edge angle. */
  vec3 v0 = get_vertex_pos(pos_nor[start_loop_index + 0]);
  vec3 v1 = get_vertex_pos(pos_nor[start_loop_index + 1]);
  vec3 v2 = get_vertex_pos(pos_nor[start_loop_index + 2]);
  vec3 face_normal = normalize(cross(v1 - v0, v2 - v0));

  vec4 edge_facs = vec4(0.0);
  for (int i = 0; i < 4; i++) {
    edge_facs[i] = compute_line_factor(start_loop_index, i, face_normal);
  }

#ifdef GPU_AMD_DRIVER_BYTE_BUG
  write_vec4(start_loop_index, edge_facs);
#else
  /* When packed into bytes, the index is the same as for the quad. */
  write_vec4(quad_index, edge_facs);
#endif
}
