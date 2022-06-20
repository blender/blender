
/* To be compiled with common_subdiv_lib.glsl */

layout(std430, binding = 1) readonly buffer inputVertexData
{
  PosNorLoop pos_nor[];
};

layout(std430, binding = 2) readonly buffer extraCoarseFaceData
{
  uint extra_coarse_face_data[];
};

layout(std430, binding = 3) readonly buffer inputVertOrigIndices
{
  int input_vert_origindex[];
};

layout(std430, binding = 4) writeonly buffer outputLoopNormals
{
  LoopNormal output_lnor[];
};

bool is_face_selected(uint coarse_quad_index)
{
  return (extra_coarse_face_data[coarse_quad_index] & coarse_face_select_mask) != 0;
}

bool is_face_hidden(uint coarse_quad_index)
{
  return (extra_coarse_face_data[coarse_quad_index] & coarse_face_hidden_mask) != 0;
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

  uint coarse_quad_index = coarse_polygon_index_from_subdiv_quad_index(quad_index,
                                                                       coarse_poly_count);

  if ((extra_coarse_face_data[coarse_quad_index] & coarse_face_smooth_mask) != 0) {
    /* Face is smooth, use vertex normals. */
    for (int i = 0; i < 4; i++) {
      PosNorLoop pos_nor_loop = pos_nor[start_loop_index + i];
      output_lnor[start_loop_index + i] = get_normal_and_flag(pos_nor_loop);
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

    LoopNormal loop_normal;
    loop_normal.nx = face_normal.x;
    loop_normal.ny = face_normal.y;
    loop_normal.nz = face_normal.z;

    for (int i = 0; i < 4; i++) {
      int origindex = input_vert_origindex[start_loop_index + i];
      float flag = 0.0;
      /* Flag for paint mode overlay and normals drawing in edit-mode. */
      if (is_face_hidden(coarse_quad_index) || (is_edit_mode && origindex == -1)) {
        flag = -1.0;
      }
      else if (is_face_selected(coarse_quad_index)) {
        flag = 1.0;
      }
      loop_normal.flag = flag;

      output_lnor[start_loop_index + i] = loop_normal;
    }
  }
}
