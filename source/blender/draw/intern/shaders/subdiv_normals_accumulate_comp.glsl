/* SPDX-FileCopyrightText: 2021-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Accumulate vertex normals from their adjacent faces.
 *
 * Accumulated normals needs to be finalized `subdiv_normals_finalize_comp.glsl`.
 * to be stored as loops.
 */

#include "subdiv_lib.glsl"

COMPUTE_SHADER_CREATE_INFO(subdiv_normals_accumulate)

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
  if (vertex_index >= shader_data.total_dispatch_size) {
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
      PosNorLoop vertex_data = pos_nor[start_loop_index + j];
      verts[j] = subdiv_get_vertex_pos(vertex_data);
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
