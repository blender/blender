/* SPDX-FileCopyrightText: 2021-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "subdiv_infos.hh"

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

float2 decode_uv(uint encoded_uv)
{
  float u = float((encoded_uv >> 16) & 0xFFFFu) / 65535.0f;
  float v = float(encoded_uv & 0xFFFFu) / 65535.0f;
  return float2(u, v);
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

float3 subdiv_position_to_float3(Position position)
{
  return float3(position.x, position.y, position.z);
}

void add_newell_cross_v3_v3v3(inout float3 n, float3 v_prev, float3 v_curr)
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
