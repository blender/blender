
/* To be compile with common_subdiv_lib.glsl */

layout(std430, binding = 1) readonly buffer inputVertexData
{
  PosNorLoop pos_nor[];
};

layout(std430, binding = 2) readonly buffer extraCoarseFaceData
{
  uint extra_coarse_face_data[];
};

layout(std430, binding = 3) writeonly buffer outputLoopNormals
{
  vec3 output_lnor[];
};

void main()
{
  /* We execute for each quad. */
  uint quad_index = get_global_invocation_index();
  if (quad_index >= total_dispatch_size) {
    return;
  }

  /* The start index of the loop is quad_index * 4. */
  uint start_loop_index = quad_index * 4;

  uint coarse_quad_index = coarse_polygon_index_from_subdiv_quad_index(quad_index,
                                                                       coarse_poly_count);

  if ((extra_coarse_face_data[coarse_quad_index] & coarse_face_smooth_mask) != 0) {
    /* Face is smooth, use vertex normals. */
    for (int i = 0; i < 4; i++) {
      PosNorLoop pos_nor_loop = pos_nor[start_loop_index + i];
      output_lnor[start_loop_index + i] = get_vertex_nor(pos_nor_loop);
    }
  }
  else {
    vec3 v0 = get_vertex_pos(pos_nor[start_loop_index + 0]);
    vec3 v1 = get_vertex_pos(pos_nor[start_loop_index + 1]);
    vec3 v2 = get_vertex_pos(pos_nor[start_loop_index + 2]);
    vec3 v3 = get_vertex_pos(pos_nor[start_loop_index + 3]);

    vec3 face_normal = vec3(0.0);
    add_newell_cross_v3_v3v3(face_normal, v0, v1);
    add_newell_cross_v3_v3v3(face_normal, v1, v2);
    add_newell_cross_v3_v3v3(face_normal, v2, v3);
    add_newell_cross_v3_v3v3(face_normal, v3, v0);

    face_normal = normalize(face_normal);
    for (int i = 0; i < 4; i++) {
      output_lnor[start_loop_index + i] = face_normal;
    }
  }
}
