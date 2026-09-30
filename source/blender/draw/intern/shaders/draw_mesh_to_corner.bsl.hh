/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * Copy mesh data from vertices, edges, or faces to face corners on the GPU, so that only the
 * smaller source domain has to be uploaded from the host. The data is copied as opaque 32-bit
 * words, so any vertex format with a stride that is a multiple of 4 bytes is supported without
 * type specific shaders.
 */

#pragma once

#include "draw_defines.hh"

#include "gpu_shader_compat.hh"

namespace draw::mesh {

/**
 * Both shaders process a batch of `count` elements starting at `start`, to stay within the
 * potentially limited GPU work group count.
 */

struct GatherResources {
  [[storage(0, read)]] const uint (&src)[];
  /** The source element for each face corner, e.g. #Mesh::corner_verts(). */
  [[storage(1, read)]] const int (&indices)[];
  [[storage(2, write)]] uint (&dst)[];

  [[push_constant]] const int start;
  [[push_constant]] const int count;
  [[push_constant]] const int words_per_element;
};

/** `dst[i] = src[indices[i]]`. One thread processes one face corner. */
[[compute, local_size(DRW_MESH_TO_CORNER_GROUP_SIZE)]]
void gather_main([[resource_table]] GatherResources &srt,
                 [[global_invocation_id]] const uint3 global_id)
{
  if (global_id.x >= uint(srt.count)) {
    return;
  }
  const uint i = uint(srt.start) + global_id.x;
  const uint words_num = uint(srt.words_per_element);
  const uint src_start = uint(srt.indices[i]) * words_num;
  const uint dst_start = i * words_num;
  for (uint word = 0u; word < words_num; word++) {
    srt.dst[dst_start + word] = srt.src[src_start + word];
  }
}

struct ScatterFacesResources {
  [[storage(0, read)]] const uint (&src)[];
  /** The start of each face's range of corners, e.g. #Mesh::face_offsets(). */
  [[storage(1, read)]] const uint (&face_offsets)[];
  [[storage(2, write)]] uint (&dst)[];

  [[push_constant]] const int start;
  [[push_constant]] const int count;
  [[push_constant]] const int words_per_element;
};

/** Copy each face's value to all of its corners. One thread processes one face. */
[[compute, local_size(DRW_MESH_TO_CORNER_GROUP_SIZE)]]
void scatter_faces_main([[resource_table]] ScatterFacesResources &srt,
                        [[global_invocation_id]] const uint3 global_id)
{
  if (global_id.x >= uint(srt.count)) {
    return;
  }
  const uint face = uint(srt.start) + global_id.x;
  const uint words_num = uint(srt.words_per_element);
  const uint src_start = face * words_num;
  for (uint corner = srt.face_offsets[face]; corner < srt.face_offsets[face + 1]; corner++) {
    const uint dst_start = corner * words_num;
    for (uint word = 0u; word < words_num; word++) {
      srt.dst[dst_start + word] = srt.src[src_start + word];
    }
  }
}

PipelineCompute gather(gather_main);
PipelineCompute scatter_faces(scatter_faces_main);

}  // namespace draw::mesh
