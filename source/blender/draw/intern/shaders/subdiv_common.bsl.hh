/* SPDX-FileCopyrightText: 2021-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "draw_subdiv_shader_shared.hh"

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

#define ORIGINDEX_NONE -1

struct SubdivResources {
  [[uniform(SHADER_DATA_BUF_SLOT)]] const DRWSubdivUboStorage shader_data;

  uint get_global_invocation_index(const uint3 global_id, const uint3 num_work_groups) const
  {
    uint invocations_per_row = SUBDIV_GROUP_SIZE * num_work_groups.x;
    return global_id.x + global_id.y * invocations_per_row;
  }
};

struct PolygonOffsetBase {
  [[storage(SUBDIV_FACE_OFFSET_BUF_SLOT, read)]] uint (&subdiv_face_offset)[];

  /* Given the index of the subdivision quad, return the index of the corresponding coarse polygon.
   * This uses subdiv_face_offset and since it is a growing list of offsets, we can use binary
   * search to locate the right index. */
  uint coarse_face_index_from_subdiv_quad_index(uint subdiv_quad_index,
                                                uint coarse_face_count) const
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
};
