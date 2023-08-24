/* SPDX-FileCopyrightText: 2021-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* To be compiled with common_subdiv_lib.glsl */

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

layout(std430, binding = 3) readonly buffer vertexLoopMap
{
  uint vert_loop_map[];
};

layout(std430, binding = 4) writeonly buffer vertexNormals
{
  vec3 normals[];
};

void find_prev_and_next_vertex_on_face(
    uint face_index, uint vertex_index, out uint curr, out uint next, out uint prev)
{
  uint start_loop_index = face_index * 4;

  for (uint i = 0; i < 4; i++) {
    uint subdiv_vert_index = vert_loop_map[start_loop_index + i];

    if (subdiv_vert_index == vertex_index) {
      curr = i;
      next = (i + 1) % 4;
      prev = (i + 4 - 1) % 4;
      break;
    }
  }
}

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

    /* Compute the face normal using Newell's method. */
    vec3 verts[4];
    for (uint j = 0; j < 4; j++) {
      verts[j] = get_vertex_pos(pos_nor[start_loop_index + j]);
    }

    vec3 face_normal = vec3(0.0);
    add_newell_cross_v3_v3v3(face_normal, verts[0], verts[1]);
    add_newell_cross_v3_v3v3(face_normal, verts[1], verts[2]);
    add_newell_cross_v3_v3v3(face_normal, verts[2], verts[3]);
    add_newell_cross_v3_v3v3(face_normal, verts[3], verts[0]);

    /* Accumulate angle weighted normal. */
    uint curr_vert = 0;
    uint next_vert = 0;
    uint prev_vert = 0;
    find_prev_and_next_vertex_on_face(
        adjacent_face, vertex_index, curr_vert, next_vert, prev_vert);

    vec3 curr_co = verts[curr_vert];
    vec3 prev_co = verts[next_vert];
    vec3 next_co = verts[prev_vert];

    vec3 edvec_prev = normalize(prev_co - curr_co);
    vec3 edvec_next = normalize(curr_co - next_co);

    float fac = acos(-dot(edvec_prev, edvec_next));

    accumulated_normal += face_normal * fac;
  }

  vec3 normal = normalize(accumulated_normal);
  normals[vertex_index] = normal;
}
