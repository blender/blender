/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_array_state.hh"
#include "BLI_math_vector_types.hh"

struct Mesh;

namespace blender::bke {

/**
 * Simplifies checking if the topology of a mesh before and after an operation is the same.
 *
 * It does so by remembering the topology of the mesh. In common cases, this can be done without
 * additional copies in constant time by using implicit-sharing.
 */
class MeshTopologyState {
 private:
  ArrayState<int2> edge_verts_;
  ArrayState<int> corner_verts_;
  ArrayState<int> corner_edges_;
  ArrayState<int> face_offset_indices_;

 public:
  MeshTopologyState(const Mesh &mesh);

  /**
   * True when the topology of the given mesh is the same as the topology of the mesh the
   * constructor was called with.
   */
  bool same_topology_as(const Mesh &mesh) const;
};

}  // namespace blender::bke
