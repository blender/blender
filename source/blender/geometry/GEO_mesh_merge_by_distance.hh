/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "BLI_index_mask.hh"
#include "BLI_span.hh"

struct Mesh;

/** \file
 * \ingroup geo
 */

namespace blender::geometry {

/**
 * Merge selected vertices into other selected vertices within the \a merge_distance. The merged
 * indices favor speed over accuracy, since the results will depend on the order of the vertices.
 *
 * \returns #std::nullopt if the mesh should not be changed (no vertices are merged), in order to
 * avoid copying the input. Otherwise returns the new mesh with merged geometry.
 */
std::optional<Mesh *> mesh_merge_by_distance_all(const Mesh &mesh,
                                                 const IndexMask &selection,
                                                 float merge_distance);

/**
 * Merge selected vertices along edges to other selected vertices. Only vertices connected by edges
 * are considered for merging.
 *
 * \returns #std::nullopt if the mesh should not be changed (no vertices are merged), in order to
 * avoid copying the input. Otherwise returns the new mesh with merged geometry.
 */
std::optional<Mesh *> mesh_merge_by_distance_connected(const Mesh &mesh,
                                                       Span<bool> selection,
                                                       float merge_distance,
                                                       bool only_loose_edges);

/**
 * Merge Verts indicated in the targets map.
 *
 * This frees the given mesh and returns a new mesh.
 *
 * \param vert_dest_map: The table that maps vertices to target vertices.  a value of -1
 * indicates a vertex is a target, and is to be kept.
 * This array is aligned with 'mesh->totvert'
 * \warning \a vert_merge_map must **not** contain any chained mapping (v1 -> v2 -> v3 etc.),
 * this is not supported and will likely generate corrupted geometry.
 *
 * \param vert_dest_map_len: The number of non '-1' values in `vert_dest_map`. (not the size)
 */
Mesh *mesh_merge_verts(const Mesh &mesh, MutableSpan<int> vert_dest_map, int vert_dest_map_len);

}  // namespace blender::geometry
