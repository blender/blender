
/* To be compile with common_subdiv_lib.glsl */

layout(std430, binding = 0) readonly buffer inputVertexData
{
  PosNorLoop pos_nor[];
};

layout(std430, binding = 1) readonly buffer faceAdjacencyOffsets
{
  uint face_adjacency_offsets[];
};

layout(std430, binding = 2) readonly buffer faceAdjacencyLists
{
  uint face_adjacency_lists[];
};

layout(std430, binding = 3) writeonly buffer vertexNormals
{
  vec3 normals[];
};

void main()
{
  uint vertex_index = get_global_invocation_index();
  if (vertex_index >= total_dispatch_size) {
    return;
  }

  uint first_adjacent_face_offset = face_adjacency_offsets[vertex_index];
  uint number_of_adjacent_faces = face_adjacency_offsets[vertex_index + 1] -
                                  first_adjacent_face_offset;

  vec3 accumulated_normal = vec3(0.0);

  /* For each adjacent face. */
  for (uint i = 0; i < number_of_adjacent_faces; i++) {
    uint adjacent_face = face_adjacency_lists[first_adjacent_face_offset + i];
    uint start_loop_index = adjacent_face * 4;

    /* Compute face normal. */
    vec3 adjacent_verts[3];
    for (uint j = 0; j < 3; j++) {
      adjacent_verts[j] = get_vertex_pos(pos_nor[start_loop_index + j]);
    }

    vec3 face_normal = normalize(
        cross(adjacent_verts[1] - adjacent_verts[0], adjacent_verts[2] - adjacent_verts[0]));
    accumulated_normal += face_normal;
  }

  float weight = 1.0 / float(number_of_adjacent_faces);
  vec3 normal = normalize(accumulated_normal);
  normals[vertex_index] = normal;
}
