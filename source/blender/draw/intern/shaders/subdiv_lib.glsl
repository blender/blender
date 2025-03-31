/* SPDX-FileCopyrightText: 2021-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "subdiv_info.hh"

#ifdef SUBDIV_POLYGON_OFFSET
COMPUTE_SHADER_CREATE_INFO(subdiv_polygon_offset_base)
#else
COMPUTE_SHADER_CREATE_INFO(subdiv_base)
#endif

uint get_global_invocation_index()
{
  uint invocations_per_row = gl_WorkGroupSize.x * gl_NumWorkGroups.x;
  return gl_GlobalInvocationID.x + gl_GlobalInvocationID.y * invocations_per_row;
}

vec2 decode_uv(uint encoded_uv)
{
  float u = float((encoded_uv >> 16) & 0xFFFFu) / 65535.0;
  float v = float(encoded_uv & 0xFFFFu) / 65535.0;
  return vec2(u, v);
}

bool is_set(uint i)
{
  /* QuadNode.Child.isSet is the first bit of the bit-field. */
  return (i & 0x1u) != 0;
}

bool is_leaf(uint i)
{
  /* QuadNode.Child.isLeaf is the second bit of the bit-field. */
  return (i & 0x2u) != 0;
}

uint get_index(uint i)
{
  /* QuadNode.Child.index is made of the remaining bits. */
  return (i >> 2) & 0x3FFFFFFFu;
}

PosNorLoop subdiv_set_vertex_pos(PosNorLoop in_vertex_data, vec3 pos)
{
  in_vertex_data.x = pos.x;
  in_vertex_data.y = pos.y;
  in_vertex_data.z = pos.z;
  return in_vertex_data;
}

/* Set the vertex normal but preserve the existing flag. This is for when we compute manually the
 * vertex normals when we cannot use the limit surface, in which case the flag and the normal are
 * set by two separate compute pass. */
PosNorLoop subdiv_set_vertex_nor(PosNorLoop in_vertex_data, vec3 nor)
{
  in_vertex_data.nx = nor.x;
  in_vertex_data.ny = nor.y;
  in_vertex_data.nz = nor.z;
  return in_vertex_data;
}

PosNorLoop subdiv_set_vertex_flag(PosNorLoop in_vertex_data, float flag)
{
  in_vertex_data.flag = flag;
  return in_vertex_data;
}

vec3 subdiv_get_vertex_pos(PosNorLoop vertex_data)
{
  return vec3(vertex_data.x, vertex_data.y, vertex_data.z);
}

LoopNormal subdiv_get_normal_and_flag(PosNorLoop vertex_data)
{
  LoopNormal loop_nor;
  loop_nor.nx = vertex_data.nx;
  loop_nor.ny = vertex_data.ny;
  loop_nor.nz = vertex_data.nz;
  loop_nor.flag = vertex_data.flag;
  return loop_nor;
}

void add_newell_cross_v3_v3v3(inout vec3 n, vec3 v_prev, vec3 v_curr)
{
  n[0] += (v_prev[1] - v_curr[1]) * (v_prev[2] + v_curr[2]);
  n[1] += (v_prev[2] - v_curr[2]) * (v_prev[0] + v_curr[0]);
  n[2] += (v_prev[0] - v_curr[0]) * (v_prev[1] + v_curr[1]);
}

#define ORIGINDEX_NONE -1

#ifdef SUBDIV_POLYGON_OFFSET

/* Given the index of the subdivision quad, return the index of the corresponding coarse polygon.
 * This uses subdiv_face_offset and since it is a growing list of offsets, we can use binary
 * search to locate the right index. */
uint coarse_face_index_from_subdiv_quad_index(uint subdiv_quad_index, uint coarse_face_count)
{
  uint first = 0;
  uint last = coarse_face_count;

  while (first != last) {
    uint middle = (first + last) / 2;

    if (subdiv_face_offset[middle] < subdiv_quad_index) {
      first = middle + 1;
    }
    else {
      last = middle;
    }
  }

  if (first < coarse_face_count && subdiv_face_offset[first] == subdiv_quad_index) {
    return first;
  }

  return first - 1;
}
#else
uint coarse_face_index_from_subdiv_quad_index(uint subdiv_quad_index, uint coarse_face_count);
#endif
