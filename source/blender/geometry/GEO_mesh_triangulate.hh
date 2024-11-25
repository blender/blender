/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "BLI_index_mask.hh"

#include "BKE_attribute.hh"

struct Mesh;

namespace blender::geometry {

/** \warning Values are saved in files. */
enum class TriangulateNGonMode {
  /** Add a "beauty" pass on top of the standard ear-clipping algorithm. */
  Beauty = 0,
  EarClip = 1,
};

/** \warning Values are saved in files. */
enum class TriangulateQuadMode {
  /** Complex method to determine the best looking edge. */
  Beauty = 0,
  /** Create a new edge from the first corner to the last. */
  Fixed = 1,
  /** Create a new edge from the second corner to the third. */
  Alternate = 2,
  /** Create a new edge along the shortest diagonal. */
  ShortEdge = 3,
  /** Create a new edge along the longest diagonal. */
  LongEdge = 4,
};

/**
 * \return #std::nullopt if the mesh is not changed (when every selected face is already a
 * triangle).
 */
std::optional<Mesh *> mesh_triangulate(const Mesh &src_mesh,
                                       const IndexMask &selection,
                                       TriangulateNGonMode ngon_mode,
                                       TriangulateQuadMode quad_mode,
                                       const bke::AttributeFilter &attribute_filter);

}  // namespace blender::geometry
