/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * Copy mesh data from vertices, edges, or faces to face corners on the GPU, so that only the
 * smaller source domain has to be uploaded from the host. The data is copied as opaque 32-bit
 * words, so any vertex format with a stride of 4, 8, 12, or 16 bytes is supported without type
 * specific shaders.
 */

#pragma once

#include "draw_defines.hh"

#include "draw_attribute_shader_shared.hh"
#include "gpu_shader_compat.hh"

namespace draw::mesh {

/**
 * Both shaders process a batch of `count` elements starting at `start`, to stay within the
 * potentially limited GPU work group count.
 */

template<typename T> struct GatherResources {
  [[storage(0, read)]] const T (&src)[];
  /** The source element for each face corner, e.g. #Mesh::corner_verts(). */
  [[storage(1, read)]] const int (&indices)[];
  [[storage(2, write)]] T (&dst)[];

  [[push_constant]] const int start;
  [[push_constant]] const int count;
};

template struct GatherResources<StoredUint>;
template struct GatherResources<StoredUint2>;
template struct GatherResources<StoredUint3>;
template struct GatherResources<StoredUint4>;

/** `dst[i] = src[indices[i]]`. One thread processes one face corner. */
template<typename T>
[[compute, local_size(DRW_MESH_TO_CORNER_GROUP_SIZE)]]
void gather_main([[resource_table]] GatherResources<T> &srt,
                 [[global_invocation_id]] const uint3 global_id)
{
  if (global_id.x >= uint(srt.count)) {
    return;
  }
  const uint i = uint(srt.start) + global_id.x;
  srt.dst[i] = srt.src[srt.indices[i]];
}

template void gather_main<StoredUint>(GatherResources<StoredUint> &, const uint3);
template void gather_main<StoredUint2>(GatherResources<StoredUint2> &, const uint3);
template void gather_main<StoredUint3>(GatherResources<StoredUint3> &, const uint3);
template void gather_main<StoredUint4>(GatherResources<StoredUint4> &, const uint3);

template<typename T> struct ScatterFacesResources {
  [[storage(0, read)]] const T (&src)[];
  /** The start of each face's range of corners, e.g. #Mesh::face_offsets(). */
  [[storage(1, read)]] const uint (&face_offsets)[];
  [[storage(2, write)]] T (&dst)[];

  [[push_constant]] const int start;
  [[push_constant]] const int count;
};

template struct ScatterFacesResources<StoredUint>;
template struct ScatterFacesResources<StoredUint2>;
template struct ScatterFacesResources<StoredUint3>;
template struct ScatterFacesResources<StoredUint4>;

/** Copy each face's value to all of its corners. One thread processes one face. */
template<typename T>
[[compute, local_size(DRW_MESH_TO_CORNER_GROUP_SIZE)]]
void scatter_faces_main([[resource_table]] ScatterFacesResources<T> &srt,
                        [[global_invocation_id]] const uint3 global_id)
{
  if (global_id.x >= uint(srt.count)) {
    return;
  }
  const uint face = uint(srt.start) + global_id.x;
  const T value = srt.src[face];
  for (uint corner = srt.face_offsets[face]; corner < srt.face_offsets[face + 1]; corner++) {
    srt.dst[corner] = value;
  }
}

template void scatter_faces_main<StoredUint>(ScatterFacesResources<StoredUint> &, const uint3);
template void scatter_faces_main<StoredUint2>(ScatterFacesResources<StoredUint2> &, const uint3);
template void scatter_faces_main<StoredUint3>(ScatterFacesResources<StoredUint3> &, const uint3);
template void scatter_faces_main<StoredUint4>(ScatterFacesResources<StoredUint4> &, const uint3);

PipelineCompute gather_4(gather_main<StoredUint>);
PipelineCompute gather_8(gather_main<StoredUint2>);
PipelineCompute gather_12(gather_main<StoredUint3>);
PipelineCompute gather_16(gather_main<StoredUint4>);
PipelineCompute scatter_faces_4(scatter_faces_main<StoredUint>);
PipelineCompute scatter_faces_8(scatter_faces_main<StoredUint2>);
PipelineCompute scatter_faces_12(scatter_faces_main<StoredUint3>);
PipelineCompute scatter_faces_16(scatter_faces_main<StoredUint4>);

}  // namespace draw::mesh
