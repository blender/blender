/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"

#include "BKE_attribute.hh"
#include "BKE_mesh_topology_state.hh"
#include "BKE_mesh_types.hh"

namespace blender::bke {

template<typename T> ArrayState<T> attribute_reader_to_array_state(const AttributeReader<T> &attr)
{
  if (!attr) {
    return {};
  }
  return {attr.varray, attr.sharing_info};
}

template<typename T>
bool attribute_reader_matches_array_state(const ArrayState<T> &array_state,
                                          const AttributeReader<T> &attr)
{
  if (!attr) {
    return array_state.is_empty();
  }
  return array_state.same_as(attr.varray, attr.sharing_info);
}

MeshTopologyState::MeshTopologyState(const Mesh &mesh)
{
  const AttributeAccessor attributes = mesh.attributes();
  edge_verts_ = attribute_reader_to_array_state(attributes.lookup<int2>(".edge_verts"));
  corner_verts_ = attribute_reader_to_array_state(attributes.lookup<int>(".corner_vert"));
  corner_edges_ = attribute_reader_to_array_state(attributes.lookup<int>(".corner_edge"));
  face_offset_indices_ = ArrayState<int>(VArray<int>::from_span(mesh.face_offsets()),
                                         mesh.runtime->face_offsets_sharing_info);
}

bool MeshTopologyState::same_topology_as(const Mesh &mesh) const
{
  const AttributeAccessor attributes = mesh.attributes();
  if (!attribute_reader_matches_array_state(edge_verts_, attributes.lookup<int2>(".edge_verts"))) {
    return false;
  }
  if (!attribute_reader_matches_array_state(corner_verts_, attributes.lookup<int>(".corner_vert")))
  {
    return false;
  }
  if (!attribute_reader_matches_array_state(corner_edges_, attributes.lookup<int>(".corner_edge")))
  {
    return false;
  }
  if (!face_offset_indices_.same_as(VArray<int>::from_span(mesh.face_offsets()),
                                    mesh.runtime->face_offsets_sharing_info))
  {
    return false;
  }
  return true;
}

}  // namespace blender::bke
