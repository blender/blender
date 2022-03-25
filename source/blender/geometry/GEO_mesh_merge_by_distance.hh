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
                                                 IndexMask selection,
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

}  // namespace blender::geometry
