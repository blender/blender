/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "BKE_attribute_filter.hh"

#include "BLI_index_mask.hh"
#include "BLI_span.hh"

namespace blender {

struct Mesh;

/** \file
 * \ingroup geo
 */

namespace geometry {

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
 * Merge vertices into target vertices targets.
 *
 * \param vert_src_to_target: Maps each vertex to the vertex it merges into. A value of -1
 * indicates that the vertex is either a target or isn't merged at all, and is to be kept. The
 * array is aligned with `mesh.verts_num`.
 * \warning \a vert_src_to_target must **not** contain any chained mapping (v1 -> v2 -> v3 etc.),
 * this is not supported and will likely generate corrupted geometry.
 *
 * \param removed_verts_num: The number of non '-1' values in `vert_src_to_target`, in other words
 * the number of vertices removed by the merge (not the size of the array).
 * \param do_mix_data: If true, each group of merged vertices (the sources with the same target,
 * plus the target vertex) will have their custom data interpolated into the resulting vertex. If
 * false, only the custom data of the target vertex will remain.
 */
Mesh *mesh_merge_verts(const Mesh &mesh,
                       MutableSpan<int> vert_src_to_target,
                       int removed_verts_num,
                       bool do_mix_data);

Mesh *mesh_merge_verts(const Mesh &mesh,
                       const IndexMask &selection,
                       Span<int> merge_ids,
                       const bke::AttributeFilter &attribute_filter);

}  // namespace geometry
}  // namespace blender
